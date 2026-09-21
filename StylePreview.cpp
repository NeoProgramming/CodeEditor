// stylepreview.cpp
#include "stylepreview.h"
#include "fontmanager.h"

#include <QPainter>

StylePreview::StylePreview(QWidget *parent)
	: QWidget(parent)
{
	setMinimumHeight(100);
	setAutoFillBackground(true);
	QPalette p = palette();
	p.setColor(QPalette::Window, Qt::white);
	setPalette(p);
}

void StylePreview::setData(const QVector<SyntaxElementStyle> &elements,
	int cellWidth, int cellHeight,
	FontManager *fontManager)
{
	m_elements = elements;
	m_cellW = qMax(1, cellWidth);
	m_cellH = qMax(1, cellHeight);
	m_fontManager = fontManager;

	rebuildSample();
	update();
}

void StylePreview::rebuildSample()
{
	// Здесь мы заранее задаём «раскраску» образца:
	// какие фрагменты каким синтаксическим элементам соответствуют.
	// Это не парсер, а фиксированный сценарий для превью.

	m_sample.clear();

	auto add = [this](const QString &text, const QString &id) {
		int idx = 0;
		for (int i = 0; i < m_elements.size(); ++i) {
			if (m_elements[i].id == id) { idx = i; break; }
		}
		m_sample.append({ text, idx });
	};

	add("// Sample: ", "comment");
	add("int", "keyword");
	add(" ", "default");
	add("value", "function");
	add(" = ", "default");
	add("42", "number");
	add(";", "default");
	add("\n", "default");

	add("#include", "preproc");
	add(" <string>", "string");
	add("\n", "default");

	add("asm { mov eax, 1 }", "asm");
	add("\n", "default");
	add("$script: x = 1 + 2", "script");
	add("\n", "default");
	add("{\"k\": \"v\", \"n\": 1}", "json");
	add("\n", "default");
	add("<root><item/></root>", "xml");
	add("\n", "default");
	add("`(a b c)", "quasi");
	add("\n", "default");
	add("MACRO(x)", "macro");
	add("\n", "default");
	add("syntax-rule", "smacro");
	add("\n", "default");
}

void StylePreview::paintEvent(QPaintEvent * /*event*/)
{
	QPainter p(this);
	p.fillRect(rect(), palette().color(QPalette::Window));

	if (!m_fontManager) return;

	// Строим карту «id -> откалиброванный QFont» для текущего размера ячейки.
	// Превью использует временный FontManager, чтобы не менять основной.
	FontManager local;
	local.setCellSize(m_cellW, m_cellH);

	QVector<int> fontIndex(m_elements.size(), -1);
	for (int i = 0; i < m_elements.size(); ++i) {
		const auto &e = m_elements[i];
		fontIndex[i] = local.addFont(e.fontFamily, e.bold, e.italic);
	}

	// Отрисовка: проходим по сегментам, каждый символ рисуем в своей ячейке.
	int x = 4;
	int y = 4;
	const int top = y;

	// Фон под всю знакоместную сетку
	// (можно включить для отладки)
	// p.setPen(QColor(230,230,230));
	// ...

	for (const Segment &seg : m_sample) {
		const SyntaxElementStyle &style = m_elements[seg.elementIndex];
		QFont f = local.getFont(fontIndex[seg.elementIndex]);
		if (f.family().isEmpty())
			f = font();

		// Задаём высоту ячейки вручную — шрифт уже откалиброван под ширину
		f.setPixelSize(qMax(1, m_cellH - 2)); // небольшой внутренний отступ

		p.setFont(f);
		p.setPen(style.color);

		for (QChar ch : seg.text) {
			if (ch == QLatin1Char('\n')) {
				x = 4;
				y += m_cellH;
				continue;
			}
			// Рисуем символ по центру знакоместа
			const QRect cell(x, y, m_cellW, m_cellH);
			p.drawText(cell, Qt::AlignCenter, QString(ch));
			x += m_cellW;
		}
	}

	// Рамка области
	p.setPen(QColor(180, 180, 180));
	p.drawRect(rect().adjusted(0, 0, -1, -1));

	Q_UNUSED(top);
}
