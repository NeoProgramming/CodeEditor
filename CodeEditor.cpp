#include "codeeditor.h"
#include "highlighter.h"

#include <QFontDatabase>
#include <QPainter>
#include <QTextBlock>
#include <QtMath> 

#include "FixedHeightLayout.h"
#include "FontManager.h"

CodeEditor::CodeEditor(FontManager *fontManager,
	const QVector<SyntaxElementStyle> &elements, QWidget *parent)
	: QPlainTextEdit(parent)
	, m_fontManager(fontManager)
{
	// 1. Моноширинный шрифт виджета — берём из FontManager,
    //    а если там пусто, падаем на системный.
	if (m_fontManager && m_fontManager->fontCount() > 0) {
		setFont(m_fontManager->getFont(ELEM_DEFAULT));
	}
	else {
		QFont fallback = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		setFont(fallback);
	}

	// 2. Режим без переноса строк
	setLineWrapMode(QPlainTextEdit::NoWrap);

	// 3. Подменяем layout документа на кастомный.
	//    Высота будет установлена позже, в onFontsChanged().
	m_fixedLayout = new FixedHeightLayout(document());
	document()->setDocumentLayout(m_fixedLayout);

	// 4. Дефолтный шрифт документа — до создания Highlighter
	//    и до любого текста. Нужен, чтобы пустые блоки
	//    имели правильную высоту.
	applyDefaultFont();
	
	// 5. Полоса с номерами строк
	m_lineNumberArea = new LineNumberArea(this);
	
	connect(this, &CodeEditor::blockCountChanged,
		this, &CodeEditor::updateLineNumberAreaWidth);
	connect(this, &CodeEditor::updateRequest,
		this, &CodeEditor::updateLineNumberArea);
	connect(this, &CodeEditor::cursorPositionChanged,
		this, &CodeEditor::highlightCurrentLine);

	updateLineNumberAreaWidth(0);
	highlightCurrentLine();

	// 6. Табы, высота строки, перерисовка — всё в одном месте
	onFontsChanged();

	// 7. Highlighter — последним, когда всё остальное готово
	m_highlighter = new Highlighter(document(), m_fontManager, elements, this);
}

void CodeEditor::setSyntaxStyles(const QVector<SyntaxElementStyle> &elements)
{
	if (m_highlighter)
		m_highlighter->setStyles(elements);
}

void CodeEditor::onFontsChanged()
{
	if (m_fixedLayout && m_fontManager) {
		const qreal h = m_fontManager->recommendedLineHeight();
		m_fixedLayout->setFixedLineHeight(h);
		const int cellW = m_fontManager->getCellWidth();
		const int tabCols = 4;  // сколько знакомест занимает один таб
		setTabStopDistance(static_cast<qreal>(cellW * tabCols));

		applyDefaultFont();
	}
	viewport()->update();
}

int CodeEditor::lineNumberAreaWidth() const
{
	int digits = 1;
	int max = qMax(1, blockCount());
	while (max >= 10) {
		max /= 10;
		++digits;
	}
	int space = 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
	return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
	setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
	if (dy)
		m_lineNumberArea->scroll(0, dy);
	else
		m_lineNumberArea->update(0, rect.y(),
			m_lineNumberArea->width(), rect.height());

	if (rect.contains(viewport()->rect()))
		updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
	QPlainTextEdit::resizeEvent(event);

	QRect cr = contentsRect();
	m_lineNumberArea->setGeometry(
		QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine()
{
	QList<QTextEdit::ExtraSelection> extraSelections;

	if (!isReadOnly()) {
		QTextEdit::ExtraSelection selection;
		QColor lineColor = QColor(Qt::yellow).lighter(180);

		selection.format.setBackground(lineColor);
		selection.format.setProperty(QTextFormat::FullWidthSelection, true);
		selection.cursor = textCursor();
		selection.cursor.clearSelection();
		extraSelections.append(selection);
	}

	setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
	QPainter painter(m_lineNumberArea);
	painter.fillRect(event->rect(), QColor(240, 240, 240));

	QTextBlock block = firstVisibleBlock();
	int blockNumber = block.blockNumber();
	int top = static_cast<int>(
		blockBoundingGeometry(block).translated(contentOffset()).top());
	int bottom = top + static_cast<int>(blockBoundingRect(block).height());

	while (block.isValid() && top <= event->rect().bottom()) {
		if (block.isVisible() && bottom >= event->rect().top()) {
			QString number = QString::number(blockNumber + 1);
			painter.setPen(Qt::darkGray);
			painter.drawText(0, top,
				m_lineNumberArea->width() - 6,
				fontMetrics().height(),
				Qt::AlignRight, number);
		}

		block = block.next();
		top = bottom;
		bottom = top + static_cast<int>(blockBoundingRect(block).height());
		++blockNumber;
	}
}

void CodeEditor::applyDefaultFont()
{
	if (!m_fontManager) return;

	// Ищем шрифт с максимальной высотой
	QFont tallest;
	int maxH = 0;
	for (int i = 0; i < m_fontManager->fontCount(); ++i) {
		const FontEntry &e = m_fontManager->getFontEntry(i);
		if (e.font.family().isEmpty()) continue;
		const int h = QFontMetrics(e.font).height();
		if (h > maxH) {
			maxH = h;
			tallest = e.font;
		}
	}

	if (maxH > 0)
		document()->setDefaultFont(tallest);
}
