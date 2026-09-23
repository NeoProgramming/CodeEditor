#include "highlighter.h"
#include "fontmanager.h"

#include <QTextDocument>

Highlighter::Highlighter(QTextDocument *doc,
	FontManager *fontManager,
	const QVector<SyntaxElementStyle> &elements,
	QObject *parent)
	: QSyntaxHighlighter(doc)
	, m_fontManager(fontManager)
	, m_elements(elements)
{
	Q_UNUSED(parent);

	rebuildFormatCache();
	buildRules();

	// Начальные/конечные маркеры многострочных комментариев
	m_commentStart = QRegularExpression(QStringLiteral("/\\*"));
	m_commentEnd = QRegularExpression(QStringLiteral("\\*/"));
}

// ---------------------------------------------------------------- styles

void Highlighter::setStyles(const QVector<SyntaxElementStyle> &elements,
	FontManager *fontManager)
{
	m_elements = elements;
	m_fontManager = fontManager;

	rebuildFormatCache();
	buildRules();      // правила не меняются, но пусть будут свежими

	rehighlight();
}

// ---------------------------------------------------------------- format cache

void Highlighter::rebuildFormatCache()
{
	m_formatCache.clear();
	m_formatCache.resize(m_elements.size());

	for (int i = 0; i < m_elements.size(); ++i) {
		const SyntaxElementStyle &e = m_elements[i];

		QTextCharFormat fmt;
		fmt.setForeground(e.color);

		// Шрифт: если FontManager отдал шрифт по индексу i — используем.
		// Если индекс вне диапазона (m_fonts короче m_elements) — оставляем
		// шрифт документа по умолчанию.
		if (m_fontManager && i < m_fontManager->fontCount()) {
			const QFont f = m_fontManager->getFont(i);
			if (!f.family().isEmpty())
				fmt.setFont(f);
		}

		m_formatCache[i] = fmt;
	}
}

// ---------------------------------------------------------------- rules

void Highlighter::buildRules()
{
	m_rules.clear();

	// Вспомогательная лямбда: добавить правило «шаблон -> индекс элемента»
	auto addRule = [this](const QString &pattern, int elementIndex) {
		Rule r;
		r.pattern = QRegularExpression(pattern);
		r.elementIndex = elementIndex;
		m_rules.append(r);
	};

	// --- Ключевые слова ---
	const QStringList keywords = {
		"\\bchar\\b", "\\bclass\\b", "\\bconst\\b", "\\bdouble\\b",
		"\\benum\\b", "\\bexplicit\\b", "\\bfriend\\b", "\\binline\\b",
		"\\bint\\b", "\\blong\\b", "\\bnamespace\\b", "\\boperator\\b",
		"\\bprivate\\b", "\\bprotected\\b", "\\bpublic\\b", "\\bshort\\b",
		"\\bsignals\\b", "\\bsigned\\b", "\\bslots\\b", "\\bstatic\\b",
		"\\bstruct\\b", "\\btemplate\\b", "\\btypedef\\b", "\\btypename\\b",
		"\\bunion\\b", "\\bunsigned\\b", "\\bvirtual\\b", "\\bvoid\\b",
		"\\bvolatile\\b", "\\bbool\\b", "\\bauto\\b", "\\bnew\\b",
		"\\bdelete\\b", "\\breturn\\b", "\\bif\\b", "\\belse\\b",
		"\\bfor\\b", "\\bwhile\\b", "\\bdo\\b", "\\bswitch\\b",
		"\\bcase\\b", "\\bbreak\\b", "\\bcontinue\\b", "\\btrue\\b",
		"\\bfalse\\b", "\\bnullptr\\b", "\\bthis\\b"
	};
	for (const QString &kw : keywords)
		addRule(kw, ELEM_KEYWORD);

	// --- Типы / классы (слова с большой буквы) ---
	addRule("\\b[A-Z][A-Za-z0-9_]*\\b", ELEM_TYPE);

	// --- Функции: имя перед '(' ---
	addRule("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()", ELEM_FUNCTION);

	// --- Числа ---
	addRule("\\b\\d+(\\.\\d+)?\\b", ELEM_NUMBER);

	// --- Строки в двойных кавычках ---
	addRule("\"[^\"\\\\]*(\\\\.[^\"\\\\]*)*\"", ELEM_STRING);

	// --- Символьные литералы ---
	addRule("'[^'\\\\]*(\\\\.[^'\\\\]*)*'", ELEM_CHAR);

	// --- Препроцессор ---
	addRule("^\\s*#[^\\n]*", ELEM_PREPROC);

	// --- Однострочные комментарии ---
	addRule("//[^\\n]*", ELEM_COMMENT);

	// --- Пример для ассемблерных вставок (asm { ... } или asm "..." ) ---
	// Здесь мы ловим тело asm-блоков как строку с ключевым словом asm.
	// Это упрощённый вариант; при необходимости можно усложнить.
	addRule("\\basm\\s*\\{[^}]*\\}", ELEM_ASM);
	addRule("\\basm\\s*\"[^\"]*\"", ELEM_ASM);

	// --- Скриптовые вставки: $script ... ; ---
	addRule("\\$script:[^\\n]*", ELEM_SCRIPT);

	// --- JSON-вставки: {...} в кавычках не трогаем, только простой случай ---
	addRule("\\{\\s*\"[^\"\\n]*\"\\s*:\\s*[^\\n}]*\\}", ELEM_JSON);

	// --- XML-вставки: <tag ...> ... </tag> ---
	addRule("<[A-Za-z_][A-Za-z0-9_\\-]*(\\s[^<>]*)?>", ELEM_XML);
	addRule("</[A-Za-z_][A-Za-z0-9_\\-]*\\s*>", ELEM_XML);

	// --- Квазицитирование: `(...) ---
	addRule("`\\([^)]*\\)", ELEM_QUASI);

	// --- Лексические макросы: MACRO(...) ---
	addRule("\\b[A-Z_][A-Z0-9_]*\\s*\\([^)]*\\)", ELEM_MACRO);

	// --- Синтаксические макросы: что-то вроде syntax-rule ---
	addRule("\\bsyntax-[A-Za-z_][A-Za-z0-9_\\-]*", ELEM_SMACRO);
}

// ---------------------------------------------------------------- highlight

void Highlighter::highlightBlock(const QString &text)
{
	// 1. Сброс состояния для многострочных комментариев
	setCurrentBlockState(0);

	// 2. Однострочные правила
	for (const Rule &rule : m_rules) {
		QRegularExpressionMatchIterator it =
			rule.pattern.globalMatch(text);

		while (it.hasNext()) {
			const QRegularExpressionMatch m = it.next();
			setFormat(m.capturedStart(),
				m.capturedLength(),
				m_formatCache[rule.elementIndex]);
		}
	}

	// 3. Многострочные комментарии /* ... */
	highlightMultiLineComment(text);
}

void Highlighter::highlightMultiLineComment(const QString &text)
{
	// Ищем границы /* ... */ с учётом состояния previousBlockState().
	// Состояние 1 означает "мы внутри многострочного комментария".

	int startIndex = 0;
	if (previousBlockState() != 1)
		startIndex = text.indexOf(m_commentStart);
	else
		startIndex = 0;

	while (startIndex >= 0) {
		const QRegularExpressionMatch endMatch =
			m_commentEnd.match(text, startIndex);
		int endIndex = endMatch.capturedStart();
		int commentLength;

		if (endIndex == -1) {
			// Комментарий не закрыт — тянется до конца блока
			setCurrentBlockState(1);
			commentLength = text.length() - startIndex;
		}
		else {
			// Закрыт в этом же блоке
			commentLength = endIndex - startIndex
				+ endMatch.capturedLength();
		}

		setFormat(startIndex, commentLength, m_formatCache[ELEM_COMMENT]);

		startIndex = text.indexOf(m_commentStart,
			startIndex + commentLength);
	}
}
