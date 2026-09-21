#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>

#include "syntaxstyle.h"
#include "FontManager.h"

class Highlighter : public QSyntaxHighlighter
{
	Q_OBJECT

public:
	explicit Highlighter(QTextDocument *parent = nullptr);

	void Highlighter::setStyles(const QVector<SyntaxElementStyle> &styles,
		FontManager *fm,
		const QHash<QString, int> &elementFontIndex);

protected:
	void highlightBlock(const QString &text) override;

private:
	struct Rule {
		QRegularExpression pattern;
		QTextCharFormat format;
	};

	QVector<Rule> rules;

	// Многострочные комментарии /* ... */
	QRegularExpression commentStart;
	QRegularExpression commentEnd;
	QTextCharFormat multiLineCommentFormat;
};
