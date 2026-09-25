#pragma once

#include <QPlainTextEdit>
#include "syntaxstyle.h"

class FontManager;
class Highlighter;
class LineNumberArea;
class FixedHeightLayout;

class CodeEditor : public QPlainTextEdit
{
	Q_OBJECT

public:
	explicit CodeEditor(FontManager *fontManager,
		const QVector<SyntaxElementStyle> &elements, QWidget *parent = nullptr);

	// Обновить стили подсветки
	void setSyntaxStyles(const QVector<SyntaxElementStyle> &elements);

	// Обновить высоту строки после смены шрифтов / размера ячейки
	void onFontsChanged();

	void lineNumberAreaPaintEvent(QPaintEvent *event);
	int lineNumberAreaWidth() const;

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void updateLineNumberAreaWidth(int newBlockCount);
	void updateLineNumberArea(const QRect &rect, int dy);
	void highlightCurrentLine();

private:
	Highlighter  *m_highlighter = nullptr;
	FixedHeightLayout *m_fixedLayout = nullptr;
	FontManager  *m_fontManager = nullptr;
	LineNumberArea *lineNumberArea = nullptr;
};

// -------- Вспомогательный виджет для полосы с номерами строк --------
class LineNumberArea : public QWidget
{
public:
	explicit LineNumberArea(CodeEditor *editor)
		: QWidget(editor), codeEditor(editor) {}

	QSize sizeHint() const override {
		return QSize(codeEditor->lineNumberAreaWidth(), 0);
	}

protected:
	void paintEvent(QPaintEvent *event) override {
		codeEditor->lineNumberAreaPaintEvent(event);
	}

private:
	CodeEditor *codeEditor;
};
