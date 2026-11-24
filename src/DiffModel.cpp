#include "DiffModel.h"

#include <QStringList>
#include <QRegularExpression>
#include <algorithm>
#include <limits>

// ============================================================
// Вспомогательное: нормализация строки для сравнения
// ============================================================

namespace {

	QString normalizeForCompare(const QString &line, bool ignoreWhitespace)
	{
		// \r\n / \r -> \n (на случай, если текст пришёл с Windows-переводами)
		QString s = line;
		s.remove('\r');

		if (ignoreWhitespace) {
			// Убираем trailing whitespace и ведущие/хвостовые пробелы
			s = s.trimmed();
			// Внутренние множественные пробелы -> один (агрессивно; можно убрать)
			static const QRegularExpression ws(QStringLiteral("\\s+"));
			s.replace(ws, QStringLiteral(" "));
		}
		return s;
	}

	// Разбить текст на строки. QTextDocument::toPlainText() всегда возвращает
	// текст без завершающего \n; split('\n') даст корректное число строк.
	QStringList splitLines(const QString &text)
	{
		if (text.isEmpty())
			return { QString() };   // пустой документ = одна пустая строка?
		return text.split(QLatin1Char('\n'));
	}

	// ============================================================
	// LCS по строкам
	// ============================================================

	// Возвращает пары (i, j) совпадающих строк в порядке возрастания.
	// Обычный DP-подход O(N*M) памяти; для первой итерации достаточно.
	// Перед вызовом префикс/суффикс уже обрезаны, поэтому N, M — только
	// различающаяся середина.
	QVector<QPair<int, int>> lcsMatch(
		const QVector<QString> &a,
		const QVector<QString> &b)
	{
		const int n = a.size();
		const int m = b.size();
		if (n == 0 || m == 0)
			return {};

		// dp[i][j] = длина LCS для a[i..] и b[j..]
		// Храним как плоский массив для скорости.
		QVector<int> dp((n + 1) * (m + 1), 0);
		auto at = [&](int i, int j) -> int& { return dp[i * (m + 1) + j]; };

		for (int i = n - 1; i >= 0; --i) {
			for (int j = m - 1; j >= 0; --j) {
				if (a[i] == b[j])
					at(i, j) = at(i + 1, j + 1) + 1;
				else
					at(i, j) = std::max(at(i + 1, j), at(i, j + 1));
			}
		}

		// Восстанавливаем пары
		QVector<QPair<int, int>> pairs;
		int i = 0, j = 0;
		while (i < n && j < m) {
			if (a[i] == b[j]) {
				pairs.append({ i, j });
				++i; ++j;
			}
			else if (at(i + 1, j) >= at(i, j + 1)) {
				++i;
			}
			else {
				++j;
			}
		}
		return pairs;
	}

	// ============================================================
	// Пословный diff (для пары Replace)
	// ============================================================

	// Разбить строку на «слова» с сохранением позиций. Слово — последовательность
	// идентификаторных символов или одиночный не-пробельный символ.
	// Пробелы приклеиваются к предыдущему слову (чтобы «a b» и «a  b» давали
	// одно слово "a " и "b" — грубо, но для подсветки приемлемо).
	struct Token
	{
		int start;
		int length;
	};

	QVector<Token> tokenize(const QString &s)
	{
		QVector<Token> tokens;
		int i = 0;
		const int n = s.size();
		while (i < n) {
			const QChar c = s[i];
			if (c.isSpace()) {
				// Пробел — приклеиваем к предыдущему токену, если есть
				int start = i;
				while (i < n && s[i].isSpace()) ++i;
				if (!tokens.isEmpty()) {
					tokens.last().length += (i - start);
				}
				else {
					tokens.append({ start, i - start });
				}
			}
			else if (c.isLetterOrNumber() || c == '_') {
				int start = i;
				while (i < n && (s[i].isLetterOrNumber() || s[i] == '_')) ++i;
				tokens.append({ start, i - start });
			}
			else {
				tokens.append({ i, 1 });
				++i;
			}
		}
		return tokens;
	}

	// лишнее
	QVector<Token> tokensOf(const QString &s)
	{
		return tokenize(s);
	}

	// LCS по токенам, возвращает совпадающие пары индексов.
	QVector<QPair<int, int>> lcsTokens(
		const QVector<Token> &a, const QString &sa,
		const QVector<Token> &b, const QString &sb)
	{
		const int n = a.size();
		const int m = b.size();
		if (n == 0 || m == 0)
			return {};

		QVector<int> dp((n + 1) * (m + 1), 0);
		auto at = [&](int i, int j) -> int& { return dp[i * (m + 1) + j]; };

		for (int i = n - 1; i >= 0; --i) {
			for (int j = m - 1; j >= 0; --j) {
				const QString ta = sa.mid(a[i].start, a[i].length);
				const QString tb = sb.mid(b[j].start, b[j].length);
				if (ta == tb)
					at(i, j) = at(i + 1, j + 1) + 1;
				else
					at(i, j) = std::max(at(i + 1, j), at(i, j + 1));
			}
		}

		QVector<QPair<int, int>> pairs;
		int i = 0, j = 0;
		while (i < n && j < m) {
			const QString ta = sa.mid(a[i].start, a[i].length);
			const QString tb = sb.mid(b[j].start, b[j].length);
			if (ta == tb) {
				pairs.append({ i, j });
				++i; ++j;
			}
			else if (at(i + 1, j) >= at(i, j + 1)) {
				++i;
			}
			else {
				++j;
			}
		}
		return pairs;
	}

	// ============================================================
	// Похожесть двух строк (для решения "пара Replace или нет")
	// ============================================================

	// Доля общих токенов от максимума. 0..1.
	double similarity(const QString &a, const QString &b)
	{
		if (a == b) return 1.0;

		const QVector<Token> ta = tokenize(a);
		const QVector<Token> tb = tokenize(b);
		if (ta.isEmpty() && tb.isEmpty()) return 1.0;
		if (ta.isEmpty() || tb.isEmpty()) return 0.0;

		const auto pairs = lcsTokens(ta, a, tb, b);
		const int common = pairs.size();
		const int denom = std::max(ta.size(), tb.size());
		return denom > 0 ? double(common) / double(denom) : 0.0;
	}

	constexpr double kReplaceThreshold = 0.5;

} // namespace

// ============================================================
// DiffModel
// ============================================================

// ============================================================
// Выравнивание одного блока несовпадений
// ============================================================
//
// pFrom..pTo-1 — удалённые/заменённые primary-строки
// sFrom..sTo-1 — добавленные/заменяющие secondary-строки
//
// Возвращает последовательность AlignedLine для этого блока.
// Порядок: монотонный по p и по s.
//
// Стратегия:
//   1) жадно подобрать пары Replace (похожие строки),
//      отфильтровать так, чтобы пары были монотонны по s
//      (иначе порядок AlignedLine станет некорректным);
//   2) пройти p и s одновременно, вставляя Replace в точках пар,
//      Delete для непарных p, Insert для непарных s.
//
// Результат по построению:
//   - каждая p-строка встречается ровно один раз (Delete или Replace);
//   - каждая s-строка встречается ровно один раз (Insert или Replace);
//   - порядок в результирующем векторе соответствует порядку в файлах.

struct PairPlan {
	QVector<int> pToS;   // для p: индекс s или -1 (Delete)
};

QVector<QPair<int, int>> pairReplaces(const QVector<QString> &pMid,
	const QVector<QString> &sMid,
	int pFrom, int pTo,
	int sFrom, int sTo)
{
	QVector<bool> sUsed(sTo - sFrom, false);
	QVector<QPair<int, int>> raw;   // (p, s)

	for (int p = pFrom; p < pTo; ++p) {
		int bestS = -1;
		double bestScore = kReplaceThreshold;
		for (int s = sFrom; s < sTo; ++s) {
			if (sUsed[s - sFrom]) continue;
			const double sc = similarity(pMid[p], sMid[s]);
			if (sc > bestScore) {
				bestScore = sc;
				bestS = s;
			}
		}
		if (bestS >= 0) {
			raw.append({ p, bestS });
			sUsed[bestS - sFrom] = true;
		}
	}

	// Сортируем по p (жадность уже даёт возрастание p, но на всякий случай).
	std::sort(raw.begin(), raw.end(),
		[](const QPair<int, int> &x, const QPair<int, int> &y) {
		return x.first < y.first;
	});

	// Фильтруем немонотонные по s: оставляем только те пары,
	// где s строго больше последнего принятого s.
	QVector<QPair<int, int>> mono;
	int lastS = sFrom - 1;
	for (const auto &pr : raw) {
		if (pr.second > lastS) {
			mono.append(pr);
			lastS = pr.second;
		}
	}
	return mono;
}

// Построение AlignedLine (фабрики)
// ============================================================

AlignedLine makeEqual(const QString &pt, const QString &st,
	int pFile, int sFile)
{
	AlignedLine al;
	al.op = DiffOp::Equal;
	al.primaryText = pt;
	al.secondaryText = st;
	al.primaryFileLine = pFile;
	al.secondaryFileLine = sFile;
	return al;
}

AlignedLine makeDelete(const QString &pt, int pFile)
{
	AlignedLine al;
	al.op = DiffOp::Delete;
	al.primaryText = pt;
	al.primaryFileLine = pFile;
	return al;
}

AlignedLine makeInsert(const QString &st, int sFile)
{
	AlignedLine al;
	al.op = DiffOp::Insert;
	al.secondaryText = st;
	al.secondaryFileLine = sFile;
	return al;
}

AlignedLine makeReplace(const QString &pt, const QString &st,
	int pFile, int sFile)
{
	AlignedLine al;
	al.op = DiffOp::Replace;
	al.primaryText = pt;
	al.secondaryText = st;
	al.primaryFileLine = pFile;
	al.secondaryFileLine = sFile;
	return al;
}

// Добавить в out последовательность AlignedLine для блока несовпадений.
void emitBlock(QVector<AlignedLine> &out,
	const QVector<QString> &pMid, const QVector<QString> &sMid,
	int pFrom, int pTo, int sFrom, int sTo)
{
	if (pFrom >= pTo && sFrom >= sTo)
		return;

	// Только удаления.
	if (sFrom >= sTo) {
		for (int p = pFrom; p < pTo; ++p)
			out.append(makeDelete(pMid[p], p));
		return;
	}
	// Только вставки.
	if (pFrom >= pTo) {
		for (int s = sFrom; s < sTo; ++s)
			out.append(makeInsert(sMid[s], s));
		return;
	}

	// Смешанный блок: подбираем монотонные пары Replace.
	const QVector<QPair<int, int>> pairs =
		pairReplaces(pMid, sMid, pFrom, pTo, sFrom, sTo);

	int p = pFrom;
	int s = sFrom;
	int pairIdx = 0;

	while (p < pTo || s < sTo) {
		// Если текущая p — начало пары, и её s == текущая s — Replace.
		if (pairIdx < pairs.size()
			&& pairs[pairIdx].first == p
			&& pairs[pairIdx].second == s) {
			out.append(makeReplace(pMid[p], sMid[s], p, s));
			++p; ++s; ++pairIdx;
			continue;
		}

		// Если текущая p — начало пары, но s < парного s — сначала Insert'ы.
		if (pairIdx < pairs.size()
			&& pairs[pairIdx].first == p
			&& pairs[pairIdx].second > s) {
			out.append(makeInsert(sMid[s], s));
			++s;
			continue;
		}

		// Иначе: если p ещё есть и текущая p не в паре — Delete.
		if (p < pTo) {
			out.append(makeDelete(pMid[p], p));
			++p;
			continue;
		}

		// Остались только s — Insert'ы.
		if (s < sTo) {
			out.append(makeInsert(sMid[s], s));
			++s;
			continue;
		}

		// Сюда попасть не должны, но на всякий случай — break.
		break;
	}
}

void DiffModel::compute(const QString &primaryText,
	const QString &secondaryText,
	bool ignoreWhitespace)
{
	m_aligned.clear();
	m_hunks.clear();
	m_wordCache.clear();

	const QStringList primaryLines = splitLines(primaryText);
	const QStringList secondaryLines = splitLines(secondaryText);

	// Нормализованные версии — только для сравнения.
	QVector<QString> pNorm, sNorm;
	pNorm.reserve(primaryLines.size());
	sNorm.reserve(secondaryLines.size());
	for (const QString &l : primaryLines)   pNorm.append(normalizeForCompare(l, ignoreWhitespace));
	for (const QString &l : secondaryLines) sNorm.append(normalizeForCompare(l, ignoreWhitespace));

	// --- Общий префикс ---
	int prefix = 0;
	while (prefix < pNorm.size() && prefix < sNorm.size()
		&& pNorm[prefix] == sNorm[prefix])
		++prefix;

	// --- Общий суффикс (не пересекаясь с префиксом) ---
	int suffix = 0;
	while (suffix < pNorm.size() - prefix
		&& suffix < sNorm.size() - prefix
		&& pNorm[pNorm.size() - 1 - suffix]
		== sNorm[sNorm.size() - 1 - suffix])
		++suffix;

	const int pMidStart = prefix;
	const int pMidEnd = pNorm.size() - suffix;
	const int sMidStart = prefix;
	const int sMidEnd = sNorm.size() - suffix;

	// --- 1. Префикс: Equal ---
	for (int i = 0; i < prefix; ++i) {
		m_aligned.append(makeEqual(primaryLines[i], secondaryLines[i], i, i));
	}

	// --- 2. Середина: LCS + Replace ---
	if (pMidStart < pMidEnd || sMidStart < sMidEnd) {
		QVector<QString> pMid, sMid;
		pMid.reserve(pMidEnd - pMidStart);
		sMid.reserve(sMidEnd - sMidStart);
		for (int k = pMidStart; k < pMidEnd; ++k) pMid.append(primaryLines[k]);
		for (int k = sMidStart; k < sMidEnd; ++k) sMid.append(secondaryLines[k]);

		const QVector<QString> pNormMid = pNorm.mid(pMidStart, pMidEnd - pMidStart);
		const QVector<QString> sNormMid = sNorm.mid(sMidStart, sMidEnd - sMidStart);

		// Пары совпадающих по LCS строк — якоря.
		const QVector<QPair<int, int>> anchors = lcsMatch(pNormMid, sNormMid);

		// Идём по серединам, между якорями — блоки несовпадений.
		int prevP = 0;
		int prevS = 0;
		for (const auto &pr : anchors) {
			if (pr.first > prevP || pr.second > prevS) {
				// emitBlock принимает индексы в pMid/sMid, а также
				// заполняет primaryFileLine/secondaryFileLine как
				// pMidStart+idx / sMidStart+idx.
				// Передадим сдвиги через лямбду-обёртку.
				QVector<AlignedLine> block;
				emitBlock(block, pMid, sMid, prevP, pr.first, prevS, pr.second);
				for (AlignedLine &al : block) {
					if (al.primaryFileLine >= 0)
						al.primaryFileLine += pMidStart;
					if (al.secondaryFileLine >= 0)
						al.secondaryFileLine += sMidStart;
					m_aligned.append(al);
				}
			}
			m_aligned.append(makeEqual(
				pMid[pr.first], sMid[pr.second],
				pMidStart + pr.first, sMidStart + pr.second));
			prevP = pr.first + 1;
			prevS = pr.second + 1;
		}

		if (prevP < pMid.size() || prevS < sMid.size()) {
			QVector<AlignedLine> block;
			emitBlock(block, pMid, sMid, prevP, pMid.size(), prevS, sMid.size());
			for (AlignedLine &al : block) {
				if (al.primaryFileLine >= 0)
					al.primaryFileLine += pMidStart;
				if (al.secondaryFileLine >= 0)
					al.secondaryFileLine += sMidStart;
				m_aligned.append(al);
			}
		}
	}

	// --- 3. Суффикс: Equal ---
	for (int k = 0; k < suffix; ++k) {
		const int pi = pMidEnd + k;
		const int si = sMidEnd + k;
		m_aligned.append(makeEqual(
			primaryLines[pi], secondaryLines[si], pi, si));
	}

	// --- 4. Hunks: группы подряд идущих не-Equal ---
	int i = 0;
	while (i < m_aligned.size()) {
		if (m_aligned[i].op == DiffOp::Equal) { ++i; continue; }
		const int start = i;
		bool hasDel = false, hasIns = false;
		while (i < m_aligned.size() && m_aligned[i].op != DiffOp::Equal) {
			const DiffOp op = m_aligned[i].op;
			if (op == DiffOp::Delete || op == DiffOp::Replace) hasDel = true;
			if (op == DiffOp::Insert || op == DiffOp::Replace) hasIns = true;
			++i;
		}
		DiffHunk h;
		h.firstAlignedLine = start;
		h.lineCount = i - start;
		h.hasDeletes = hasDel;
		h.hasInserts = hasIns;
		const int idx = m_hunks.size();
		m_hunks.append(h);
		for (int k = start; k < i; ++k)
			m_aligned[k].hunkIndex = idx;
	}
}



// ---------------------------------------------------------------- texts

QString DiffModel::primaryAlignedText() const
{
	QStringList lines;
	lines.reserve(m_aligned.size());
	for (const AlignedLine &al : m_aligned) {
		if (al.op == DiffOp::Insert)
			lines.append(QString());   // заполнитель
		else
			lines.append(al.primaryText);
	}
	return lines.join(QLatin1Char('\n'));
}

QString DiffModel::secondaryAlignedText() const
{
	QStringList lines;
	lines.reserve(m_aligned.size());
	for (const AlignedLine &al : m_aligned) {
		if (al.op == DiffOp::Delete)
			lines.append(QString());   // заполнитель
		else
			lines.append(al.secondaryText);
	}
	return lines.join(QLatin1Char('\n'));
}

// ---------------------------------------------------------------- maps

int DiffModel::primaryFileLineForEditorLine(int editorLine) const
{
	if (editorLine < 0 || editorLine >= m_aligned.size()) return -1;
	const AlignedLine &al = m_aligned[editorLine];
	if (al.op == DiffOp::Insert) return -1;
	return al.primaryFileLine;
}

int DiffModel::secondaryFileLineForEditorLine(int editorLine) const
{
	if (editorLine < 0 || editorLine >= m_aligned.size()) return -1;
	const AlignedLine &al = m_aligned[editorLine];
	if (al.op == DiffOp::Delete) return -1;
	return al.secondaryFileLine;
}

int DiffModel::primaryEditorLineForFileLine(int fileLine) const
{
	if (fileLine < 0) return -1;
	for (int i = 0; i < m_aligned.size(); ++i) {
		const AlignedLine &al = m_aligned[i];
		if (al.op != DiffOp::Insert && al.primaryFileLine == fileLine)
			return i;
	}
	return -1;
}

int DiffModel::secondaryEditorLineForFileLine(int fileLine) const
{
	if (fileLine < 0) return -1;
	for (int i = 0; i < m_aligned.size(); ++i) {
		const AlignedLine &al = m_aligned[i];
		if (al.op != DiffOp::Delete && al.secondaryFileLine == fileLine)
			return i;
	}
	return -1;
}

// ---------------------------------------------------------------- hunks

int DiffModel::hunkIndexForAlignedLine(int alignedLine) const
{
	if (alignedLine < 0 || alignedLine >= m_aligned.size()) return -1;
	return m_aligned[alignedLine].hunkIndex;
}

// ---------------------------------------------------------------- word diff

WordDiff DiffModel::wordDiffFor(int alignedLineIndex) const
{
	if (alignedLineIndex < 0 || alignedLineIndex >= m_aligned.size())
		return {};

	auto it = m_wordCache.find(alignedLineIndex);
	if (it != m_wordCache.end())
		return it.value();

	const AlignedLine &al = m_aligned[alignedLineIndex];
	if (al.op != DiffOp::Replace)
		return {};

	const QString &a = al.primaryText;
	const QString &b = al.secondaryText;

	const QVector<Token> ta = tokensOf(a);
	const QVector<Token> tb = tokensOf(b);
	const auto pairs = lcsTokens(ta, a, tb, b);

	QVector<bool> aMatched(ta.size(), false);
	QVector<bool> bMatched(tb.size(), false);
	for (const auto &p : pairs) {
		aMatched[p.first] = true;
		bMatched[p.second] = true;
	}

	WordDiff wd;
	for (int i = 0; i < ta.size(); ++i) {
		if (!aMatched[i])
			wd.primaryChanged.append({ ta[i].start, ta[i].length });
	}
	for (int i = 0; i < tb.size(); ++i) {
		if (!bMatched[i])
			wd.secondaryChanged.append({ tb[i].start, tb[i].length });
	}

	m_wordCache.insert(alignedLineIndex, wd);
	return wd;
}
