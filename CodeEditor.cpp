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
	// Моноширинный шрифт
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	font.setPointSize(11);
	setFont(font);

	// Подменяем layout документа на кастомный
	m_fixedLayout = new FixedHeightLayout(document());

	// Вычисляем и фиксируем высоту строки по моноширинному шрифту
	QFontMetricsF fm(font);
	qreal lineHeight = qCeil(fm.lineSpacing());
	m_fixedLayout->setFixedLineHeight(lineHeight);

	document()->setDocumentLayout(m_fixedLayout);
	

	// Отступы и табуляция
	setTabStopDistance(4 * fontMetrics().horizontalAdvance(' '));
	setLineWrapMode(QPlainTextEdit::NoWrap);

	// Полоса с номерами строк
	lineNumberArea = new LineNumberArea(this);

	connect(this, &CodeEditor::blockCountChanged,
		this, &CodeEditor::updateLineNumberAreaWidth);
	connect(this, &CodeEditor::updateRequest,
		this, &CodeEditor::updateLineNumberArea);
	connect(this, &CodeEditor::cursorPositionChanged,
		this, &CodeEditor::highlightCurrentLine);

	updateLineNumberAreaWidth(0);
	highlightCurrentLine();

	// Подсветка синтаксиса C++
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
		lineNumberArea->scroll(0, dy);
	else
		lineNumberArea->update(0, rect.y(),
			lineNumberArea->width(), rect.height());

	if (rect.contains(viewport()->rect()))
		updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
	QPlainTextEdit::resizeEvent(event);

	QRect cr = contentsRect();
	lineNumberArea->setGeometry(
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
	QPainter painter(lineNumberArea);
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
				lineNumberArea->width() - 6,
				fontMetrics().height(),
				Qt::AlignRight, number);
		}

		block = block.next();
		top = bottom;
		bottom = top + static_cast<int>(blockBoundingRect(block).height());
		++blockNumber;
	}
}
