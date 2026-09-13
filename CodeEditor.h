#pragma once

#include <QPlainTextEdit>

class CodeEditor : public QPlainTextEdit
{
	Q_OBJECT

public:
	explicit CodeEditor(QWidget *parent = nullptr);

	void lineNumberAreaPaintEvent(QPaintEvent *event);
	int lineNumberAreaWidth() const;

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void updateLineNumberAreaWidth(int newBlockCount);
	void updateLineNumberArea(const QRect &rect, int dy);
	void highlightCurrentLine();

private:
	QWidget *lineNumberArea;
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
