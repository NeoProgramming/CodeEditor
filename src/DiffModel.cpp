#include "DiffModel.h"

#include <QStringList>
#include <QRegularExpression>
#include <algorithm>

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
	QVector<QPair<int, int>> lcsMatch(const QVector<QString> &a,
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

	QVector<Token> tokensOf(const QString &s)
	{
		return tokenize(s);
	}

	// LCS по токенам, возвращает совпадающие пары индексов.
	QVector<QPair<int, int>> lcsTokens(const QVector<Token> &a, const QString &sa,
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

		const QVector<Token> ta = tokensOf(a);
		const QVector<Token> tb = tokensOf(b);
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

void DiffModel::compute(const QString &primaryText,
	const QString &secondaryText,
	bool ignoreWhitespace)
{
	m_aligned.clear();
	m_hunks.clear();
	m_wordCache.clear();

	QStringList primaryLines = splitLines(primaryText);
	QStringList secondaryLines = splitLines(secondaryText);

	// Нормализованные версии — только для сравнения.
	QVector<QString> pNorm, sNorm;
	pNorm.reserve(primaryLines.size());
	sNorm.reserve(secondaryLines.size());
	for (const QString &l : primaryLines)   pNorm.append(normalizeForCompare(l, ignoreWhitespace));
	for (const QString &l : secondaryLines) sNorm.append(normalizeForCompare(l, ignoreWhitespace));

	// --- Обрезка общего префикса ---
	int prefix = 0;
	while (prefix < pNorm.size() && prefix < sNorm.size()
		&& pNorm[prefix] == sNorm[prefix])
		++prefix;

	// --- Обрезка общего суффикса (не пересекаясь с префиксом) ---
	int suffix = 0;
	while (suffix < pNorm.size() - prefix
		&& suffix < sNorm.size() - prefix
		&& pNorm[pNorm.size() - 1 - suffix] == sNorm[sNorm.size() - 1 - suffix])
		++suffix;

	const int pMidStart = prefix;
	const int pMidEnd = pNorm.size() - suffix;
	const int sMidStart = prefix;
	const int sMidEnd = sNorm.size() - suffix;

	// --- Строим выравнивание ---

	// 1. Префикс — Equal
	for (int i = 0; i < prefix; ++i) {
		AlignedLine al;
		al.op = DiffOp::Equal;
		al.primaryText = primaryLines[i];
		al.secondaryText = secondaryLines[i];
		al.primaryFileLine = i;
		al.secondaryFileLine = i;
		m_aligned.append(al);
	}

	// 2. Середина — LCS + пары Replace
	if (pMidStart < pMidEnd || sMidStart < sMidEnd) {
		QVector<QString> pMid; //= primaryLines.mid(pMidStart, pMidEnd - pMidStart);
		QVector<QString> sMid; //= secondaryLines.mid(sMidStart, sMidEnd - sMidStart);
		for (int k = pMidStart; k < pMidEnd; ++k) 
			pMid.append(primaryLines[k]);
		for (int k = sMidStart; k < sMidEnd; ++k) 
			sMid.append(secondaryLines[k]);

		QVector<QString> pNormMid = pNorm.mid(pMidStart, pMidEnd - pMidStart);
		QVector<QString> sNormMid = sNorm.mid(sMidStart, sMidEnd - sMidStart);

		const auto pairs = lcsMatch(pNormMid, sNormMid);

		// Идём по середине, между «якорями» LCS — блоки несовпадений.
		int pi = 0, si = 0;
		auto emitEqual = [&](int pIdx, int sIdx) {
			AlignedLine al;
			al.op = DiffOp::Equal;
			al.primaryText = pMid[pIdx];
			al.secondaryText = sMid[sIdx];
			al.primaryFileLine = pMidStart + pIdx;
			al.secondaryFileLine = sMidStart + sIdx;
			m_aligned.append(al);
		};
		auto emitBlock = [&](int pFrom, int pTo, int sFrom, int sTo) {
			// pFrom..pTo-1 удалены, sFrom..sTo-1 добавлены.
			// Жадно сопоставляем похожие пары -> Replace, остальное Delete/Insert.
			QVector<bool> pUsed(pTo - pFrom, false);
			QVector<bool> sUsed(sTo - sFrom, false);
			QVector<QPair<int, int>> replacePairs;

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
					replacePairs.append({ p, bestS });
					pUsed[p - pFrom] = true;
					sUsed[bestS - sFrom] = true;
				}
			}

			// Порядок пар — по возрастанию p (и s одновременно, т.к. LCS
			// гарантирует монотонность; но жадность может дать не монотонные s —
			// сортируем по p и проверяем).
			std::sort(replacePairs.begin(), replacePairs.end(),
				[](const QPair<int, int> &x, const QPair<int, int> &y) {
				return x.first < y.first;
			});

			// Проходим по блоку, вставляя Delete/Insert для непарных
			// и Replace для парных. Чтобы порядок был корректным,
			// пройдём по p от pFrom до pTo и по s, «съедая» пары.
			int p = pFrom;
			int s = sFrom;
			int pairIdx = 0;
			while (p < pTo || s < sTo) {
				// Если текущая p — начало пары, и её s == текущая s,
				// выдаём Replace.
				if (pairIdx < replacePairs.size()
					&& replacePairs[pairIdx].first == p
					&& replacePairs[pairIdx].second == s) {
					AlignedLine al;
					al.op = DiffOp::Replace;
					al.primaryText = pMid[p];
					al.secondaryText = sMid[s];
					al.primaryFileLine = pMidStart + p;
					al.secondaryFileLine = sMidStart + s;
					m_aligned.append(al);
					++p; ++s; ++pairIdx;
					continue;
				}
				// Иначе — если p не в паре (или пара «впереди»), Delete.
				// Но нужно не пропустить случай, когда s ждёт своей пары.
				// Упрощение: если p — в паре, но s < пары.s, значит,
				// сначала надо вставить Insert для s. И наоборот.
				bool pInPair = false;
				int pPairS = -1;
				for (int k = pairIdx; k < replacePairs.size(); ++k) {
					if (replacePairs[k].first == p) {
						pInPair = true; pPairS = replacePairs[k].second;
						break;
					}
				}
				if (pInPair) {
					// s < pPairS — вставляем Insert
					if (s < pPairS) {
						AlignedLine al;
						al.op = DiffOp::Insert;
						al.secondaryText = sMid[s];
						al.secondaryFileLine = sMidStart + s;
						m_aligned.append(al);
						++s;
					}
					else {
						// s > pPairS — это не должно случиться при монотонных
						// парах; на всякий случай — Delete.
						AlignedLine al;
						al.op = DiffOp::Delete;
						al.primaryText = pMid[p];
						al.primaryFileLine = pMidStart + p;
						m_aligned.append(al);
						++p;
					}
				}
				else {
					// p не в паре — Delete
					AlignedLine al;
					al.op = DiffOp::Delete;
					al.primaryText = pMid[p];
					al.primaryFileLine = pMidStart + p;
					m_aligned.append(al);
					++p;
				}

				// Если p закончились, а s ещё есть — Insert'ы
				if (p >= pTo) {
					while (s < sTo) {
						AlignedLine al;
						al.op = DiffOp::Insert;
						al.secondaryText = sMid[s];
						al.secondaryFileLine = sMidStart + s;
						m_aligned.append(al);
						++s;
					}
					break;
				}
			}
		};

		// Проходим по якорям LCS
		int prevP = 0, prevS = 0;
		for (const auto &pr : pairs) {
			if (pr.first > prevP || pr.second > prevS)
				emitBlock(prevP, pr.first, prevS, pr.second);
			emitEqual(pr.first, pr.second);
			prevP = pr.first + 1;
			prevS = pr.second + 1;
		}
		if (prevP < pMid.size() || prevS < sMid.size())
			emitBlock(prevP, pMid.size(), prevS, sMid.size());
	}

	// 3. Суффикс — Equal
	for (int k = 0; k < suffix; ++k) {
		const int pi = pMidEnd + k;
		const int si = sMidEnd + k;
		AlignedLine al;
		al.op = DiffOp::Equal;
		al.primaryText = primaryLines[pi];
		al.secondaryText = secondaryLines[si];
		al.primaryFileLine = pi;
		al.secondaryFileLine = si;
		m_aligned.append(al);
	}

	// --- Сборка hunks: группируем подряд идущие не-Equal ---
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
