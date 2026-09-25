#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>

#include "syntaxstyle.h"

class FontManager;

// Индексы синтаксических элементов.
// Должны совпадать с порядком в defaultSyntaxElements() и,
// соответственно, с порядком записей в FontManager.
enum SyntaxElement {
	ELEM_DEFAULT = 0,
	ELEM_KEYWORD,
	ELEM_TYPE,
	ELEM_FUNCTION,
	ELEM_STRING,
	ELEM_CHAR,
	ELEM_NUMBER,
	ELEM_COMMENT,
	ELEM_PREPROC,
	ELEM_ASM,
	ELEM_SCRIPT,
	ELEM_JSON,
	ELEM_XML,
	ELEM_QUASI,
	ELEM_MACRO,
	ELEM_SMACRO,
	ELEM_COUNT
};

class Highlighter : public QSyntaxHighlighter
{
	Q_OBJECT

public:
	explicit Highlighter(QTextDocument *doc,
		FontManager *fontManager,
		const QVector<SyntaxElementStyle> &elements = {},
		QObject *parent = nullptr);

	// Обновить стили и/или FontManager.
	// Пересобирает кэш форматов и вызывает rehighlight().
	void setStyles(const QVector<SyntaxElementStyle> &elements);

protected:
	void highlightBlock(const QString &text) override;

private:
	// Пересобирает m_formatCache по m_elements и m_fontManager
	void rebuildFormatCache(const QVector<SyntaxElementStyle> &elements);

	// ---- Правила для однострочных токенов ----
	struct Rule {
		QRegularExpression pattern;
		int                elementIndex; // индекс в m_elements / m_formatCache
	};

	void buildRules();
	void highlightMultiLineComment(const QString &text);

	// ---- Данные ----
	FontManager                *m_fontManager = nullptr;
	QVector<QTextCharFormat>    m_formatCache; // индекс = индекс элемента
	QVector<Rule>               m_rules;

	// Для многострочных /* ... */
	QRegularExpression m_commentStart;
	QRegularExpression m_commentEnd;
};
