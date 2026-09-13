#include "highlighter.h"

Highlighter::Highlighter(QTextDocument *parent)
	: QSyntaxHighlighter(parent)
{
	// --- Ключевые слова ---
	QTextCharFormat keywordFormat;
	keywordFormat.setForeground(Qt::darkBlue);
	keywordFormat.setFontWeight(QFont::Bold);

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

	for (const QString &pattern : keywords) {
		rules.append({ QRegularExpression(pattern), keywordFormat });
	}

	// --- Классы (слова с большой буквы) ---
	QTextCharFormat classFormat;
	classFormat.setForeground(Qt::darkMagenta);
	classFormat.setFontWeight(QFont::Bold);
	classFormat.setFontPointSize(18);
	rules.append({ QRegularExpression("\\b[A-Z][A-Za-z0-9_]*\\b"), classFormat });

	// --- Функции (имя перед скобкой) ---
	QTextCharFormat functionFormat;
	functionFormat.setForeground(QColor(0, 100, 0));
	rules.append({ QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()"),
				   functionFormat });

	// --- Числа ---
	QTextCharFormat numberFormat;
	numberFormat.setForeground(Qt::darkRed);
	numberFormat.setFontStretch(150);
	rules.append({ QRegularExpression("\\b\\d+(\\.\\d+)?\\b"), numberFormat });

	// --- Строки в двойных кавычках ---
	QTextCharFormat stringFormat;
	stringFormat.setForeground(Qt::darkGreen);
	rules.append({ QRegularExpression("\"[^\"]*\""), stringFormat });

	// --- Символьные литералы ---
	rules.append({ QRegularExpression("'[^']*'"), stringFormat });

	// --- Препроцессор (#include, #define и т.п.) ---
	QTextCharFormat preprocessorFormat;
	preprocessorFormat.setForeground(Qt::darkCyan);
	rules.append({ QRegularExpression("^\\s*#[^\\n]*"), preprocessorFormat });

	// --- Однострочные комментарии // ---
	QTextCharFormat singleLineCommentFormat;
	singleLineCommentFormat.setForeground(Qt::gray);
	singleLineCommentFormat.setFontItalic(true);
	rules.append({ QRegularExpression("//[^\\n]*"), singleLineCommentFormat });

	// --- Многострочные комментарии /* ... */ ---
	multiLineCommentFormat = singleLineCommentFormat;
	commentStart = QRegularExpression("/\\*");
	commentEnd = QRegularExpression("\\*/");
}

void Highlighter::highlightBlock(const QString &text)
{
	// Применяем однострочные правила
	for (const Rule &rule : rules) {
		QRegularExpressionMatchIterator it =
			rule.pattern.globalMatch(text);
		while (it.hasNext()) {
			const QRegularExpressionMatch match = it.next();
			setFormat(match.capturedStart(),
				match.capturedLength(),
				rule.format);
		}
	}

	// --- Обработка многострочных комментариев ---
	setCurrentBlockState(0);

	int startIndex = 0;
	if (previousBlockState() != 1)
		startIndex = text.indexOf(commentStart);

	while (startIndex >= 0) {
		const QRegularExpressionMatch endMatch =
			commentEnd.match(text, startIndex);
		int endIndex = endMatch.capturedStart();
		int commentLength;

		if (endIndex == -1) {
			setCurrentBlockState(1);
			commentLength = text.length() - startIndex;
		}
		else {
			commentLength = endIndex - startIndex
				+ endMatch.capturedLength();
		}

		setFormat(startIndex, commentLength, multiLineCommentFormat);
		startIndex = text.indexOf(commentStart,
			startIndex + commentLength);
	}
}
