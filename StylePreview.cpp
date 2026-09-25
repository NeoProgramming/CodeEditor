// stylepreview.cpp
#include "stylepreview.h"
#include "fontmanager.h"

#include <QPainter>
#include <QSet>

StylePreview::StylePreview(QWidget *parent)
	: QWidget(parent)
{
	setMinimumHeight(100);
	setAutoFillBackground(true);
	QPalette p = palette();
	p.setColor(QPalette::Window, Qt::white);
	setPalette(p);
}

void StylePreview::setFontManager(FontManager *fontManager)
{
	m_fontManager = fontManager;
	updateGeometry();
	update();
}

void StylePreview::refresh()
{
	updateGeometry();
	update();
}

void StylePreview::setData(const QVector<SyntaxElementStyle> &elements,
	int cellWidth, int cellHeight,
	FontManager *fontManager)
{
	m_elements = elements;
	m_cellW = qMax(1, cellWidth);
	m_cellH = qMax(1, cellHeight);
	m_fontManager = fontManager;

	update();
}



void StylePreview::paintEvent(QPaintEvent * /*event*/)
{
	QPainter p(this);
	p.fillRect(rect(), palette().color(QPalette::Window));

	if (!m_fontManager || m_fontManager->fontCount() == 0)
		return;

	// --- Собираем уникальные шрифты ---
	struct Item {
		FontKey key;
		QFont   font;
		int     index;
	};

	QVector<Item> unique;
	QSet<QString> seen;

	for (int i = 0; i < m_fontManager->fontCount(); ++i) {
		const FontEntry &e = m_fontManager->getFontEntry(i);

		if (e.font.family().isEmpty())
			continue;

		FontKey k{ e.family, e.bold, e.italic };

		const QString serialized = QString("%1|%2|%3")
			.arg(k.family)
			.arg(k.bold ? 1 : 0)
			.arg(k.italic ? 1 : 0);
		if (seen.contains(serialized))
			continue;
		seen.insert(serialized);

		unique.append({ k, e.font, i });
	}

	// --- Рисуем образец + имя шрифта для каждого ---
	const int marginX = 8;
	const int marginY = 8;
	const int lineGap = 2;

	int y = marginY;

	for (const Item &item : unique) {
		const QFont &f = item.font;
		QFontMetrics fm(f);

		const int lineH = fm.height();

		// Собираем имя шрифта
		QString label = item.key.family;
		if (item.key.bold)   label += " bold";
		if (item.key.italic) label += " italic";
		label += QString(" (%1 px)").arg(f.pixelSize());

		// Одна строка: образец + пробел + имя шрифта
		const QString line = m_sample + "  " + label;

		p.setFont(f);
		p.setPen(palette().color(QPalette::WindowText));
		p.drawText(marginX, y, width() - 2 * marginX, lineH,
			Qt::AlignLeft | Qt::AlignVCenter, line);

		y += lineH + lineGap;
	}
}
