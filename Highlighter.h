#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>

class Highlighter : public QSyntaxHighlighter
{
	Q_OBJECT

public:
	explicit Highlighter(QTextDocument *parent = nullptr);

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
