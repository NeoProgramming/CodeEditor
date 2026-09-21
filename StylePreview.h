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

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	struct Segment {
		QString text;
		int elementIndex; // индекс в m_elements
	};

	QVector<SyntaxElementStyle> m_elements;
	QVector<Segment> m_sample;
	int m_cellW = 8;
	int m_cellH = 16;
	FontManager *m_fontManager = nullptr;

	void rebuildSample();
};
