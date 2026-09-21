// syntaxstyle.h
#pragma once

#include <QColor>
#include <QFont>
#include <QString>

// ќдин синтаксический элемент (например, "Keyword", "String", "Comment")
struct SyntaxElementStyle {
	QString  id;          // внутренний идентификатор, не мен€етс€
	QString  title;       // отображаемое им€ в списке
	QString  fontFamily;  // им€ семейства (только моноширинные)
	bool     bold = false;
	bool     italic = false;
	QColor   color = Qt::black;

	bool operator==(const SyntaxElementStyle &o) const {
		return id == o.id
			&& fontFamily == o.fontFamily
			&& bold == o.bold
			&& italic == o.italic
			&& color == o.color;
	}
};

extern QVector<SyntaxElementStyle> defaultSyntaxElements();
