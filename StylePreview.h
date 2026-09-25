#pragma once

#include <QWidget>
#include <QVector>

#include "syntaxstyle.h"

class FontManager;

class StylePreview : public QWidget
{
	Q_OBJECT

public:
	explicit StylePreview(QWidget *parent = nullptr);

	void setData(const QVector<SyntaxElementStyle> &elements,
		int cellWidth, int cellHeight,
		FontManager *fontManager);
	// ќбновить Ч вызывать, если FontManager изменилс€ извне
	void refresh();
	void setFontManager(FontManager *fontManager);
protected:
	void paintEvent(QPaintEvent *event) override;

private:
	struct Segment {
		QString text;
		int elementIndex; // индекс в m_elements
	};

	// ”никальный ключ шрифта: family + bold + italic.
	// ѕозвол€ет пропустить повтор€ющиес€ шрифты.
	struct FontKey {
		QString family;
		bool bold = false;
		bool italic = false;

		bool operator==(const FontKey &o) const {
			return family == o.family && bold == o.bold && italic == o.italic;
		}
	};

	QVector<SyntaxElementStyle> m_elements;
	
	int m_cellW = 8;
	int m_cellH = 16;
	FontManager *m_fontManager = nullptr;

	QString      m_sample = QStringLiteral("ABCabc123+-.;/@#$%");

};
