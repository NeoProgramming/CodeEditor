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
	enum class LineMark { None, Delete, Insert, Replace, Current };

	explicit CodeEditor(FontManager *fontManager,
		const QVector<SyntaxElementStyle> &elements, QWidget *parent = nullptr);

	// ќбновить стили подсветки
	void setSyntaxStyles(const QVector<SyntaxElementStyle> &elements);

	// ќбновить высоту строки после смены шрифтов / размера €чейки
	void onFontsChanged();

	void lineNumberAreaPaintEvent(QPaintEvent *event);
	int lineNumberAreaWidth() const;

	void applyDefaultFont();

	void setHighlightCurrentLineEnabled(bool enabled);

	// ”становить разметку строк дл€ LineNumberArea.
	//  люч Ч номер блока (0-based). «начение Ч тип.
	void setLineMarks(const QHash<int, LineMark> &marks);
	void clearLineMarks();

	// ÷вета (можно вынести в QSettings позже)
	QColor colorForMark(LineMark m) const;
	void setBaseExtraSelections(const QList<QTextEdit::ExtraSelection> &selections);
protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void updateLineNumberAreaWidth(int newBlockCount);
	void updateLineNumberArea(const QRect &rect, int dy);
	void highlightCurrentLine();

private:
	void rebuildExtraSelections();   // m_baseSelections + жЄлта€ текуща€ строка

	QList<QTextEdit::ExtraSelection> m_baseSelections;
	bool m_highlightCurrentLineEnabled = true;
	Highlighter  *m_highlighter = nullptr;
	FixedHeightLayout *m_fixedLayout = nullptr;
	FontManager  *m_fontManager = nullptr;
	LineNumberArea *m_lineNumberArea = nullptr;
	QHash<int, LineMark> m_lineMarks;
};

// -------- ¬спомогательный виджет дл€ полосы с номерами строк --------
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
