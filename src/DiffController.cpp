#include "DiffController.h"
#include "CodeEditor.h"
#include "DiffColors.h"

#include <limits>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextBlock>
#include <QScrollBar>
#include <QTimer>
#include <QColor>
#include <QDebug>


// ---------------------------------------------------------------- ctor

DiffController::DiffController(CodeEditor *primary, CodeEditor *secondary,
                               QObject *parent)
    : QObject(parent)
    , m_primary(primary)
    , m_secondary(secondary)
{
    Q_ASSERT(m_primary && m_secondary);

	qDebug() << "DiffController: primary=" << m_primary
		<< "secondary=" << m_secondary;

    // Запоминаем «реальные» тексты (без раздутых строк).
    m_primaryText   = m_primary->toPlainText();
    m_secondaryText = m_secondary->toPlainText();

    setEditorsReadOnly(true);
    connectScrollSync();

    m_model.compute(m_primaryText, m_secondaryText, m_ignoreWhitespace);
    loadAlignedTexts();

    // Выбрать первый hunk как текущий.
    if (m_model.hunkCount() > 0)
        setCurrentHunk(0, /*scroll*/ false);

    emit hunksChanged();
}

// ---------------------------------------------------------------- recompute

void DiffController::recompute()
{
    // Если модель уже применялась — m_primaryText/m_secondaryText
    // содержат актуальное состояние. Пересчитываем по ним.
    m_model.compute(m_primaryText, m_secondaryText, m_ignoreWhitespace);
    m_currentHunk = m_model.hunkCount() > 0 ? 0 : -1;
    loadAlignedTexts();
    if (m_currentHunk >= 0)
        setCurrentHunk(m_currentHunk, /*scroll*/ false);
    emit hunksChanged();
}

void DiffController::setIgnoreWhitespace(bool ignore)
{
    if (m_ignoreWhitespace == ignore)
        return;
    m_ignoreWhitespace = ignore;
    recompute();
}

// ---------------------------------------------------------------- load texts

void DiffController::loadAlignedTexts()
{
	m_primary->setPlainText(m_model.primaryAlignedText());
	m_secondary->setPlainText(m_model.secondaryAlignedText());

	applySelections();

	m_primary->verticalScrollBar()->setValue(0);
	m_secondary->verticalScrollBar()->setValue(0);
}

// ---------------------------------------------------------------- selections

void DiffController::applyPanelSelection(
	CodeEditor *editor,
	int alignedIndex,
	const AlignedLine &al,
	bool primarySide,      // какая сторона WordDiff брать (primaryChanged/secondaryChanged)
	const QColor &background,
	CodeEditor::LineMark mark,
	QList<QTextEdit::ExtraSelection> &out,
	QHash<int, CodeEditor::LineMark> &marks)
{
	// есть ли реальная строка в этой панели (иначе заполнитель)
	const bool hasLine = primarySide 
		? (al.op != DiffOp::Insert)
		: (al.op != DiffOp::Delete);
	// нужна ли пословная подсветка
	const bool isReplace = (al.op == DiffOp::Replace);
	
	// 1. Фоновая подсветка строки (и для реальной, и для заполнителя).
	QTextEdit::ExtraSelection sel;
	sel.format.setBackground(background);
	sel.format.setProperty(QTextFormat::FullWidthSelection, true);
	QTextCursor c(editor->document()->findBlockByNumber(alignedIndex));
	c.clearSelection();
	sel.cursor = c;
	out.append(sel);

	// 2. Метка для LineNumberArea.
	marks.insert(alignedIndex, mark);

	// 3. Пословная подсветка — только для Replace (реальная строка с обеих сторон).
	if (!hasLine || !isReplace)
		return;

	const WordDiff wd = m_model.wordDiffFor(alignedIndex);
	const QVector<WordSpan> &spans = primarySide ? wd.primaryChanged
		: wd.secondaryChanged;
	if (spans.isEmpty())
		return;

	QTextBlock block = editor->document()->findBlockByNumber(alignedIndex);
	for (const WordSpan &ws : spans) {
		QTextEdit::ExtraSelection ws_sel;
		ws_sel.format.setBackground(DiffColors::word());
		QTextCursor wc(block);
		wc.setPosition(block.position() + ws.start);
		wc.setPosition(block.position() + ws.start + ws.length,
			QTextCursor::KeepAnchor);
		ws_sel.cursor = wc;
		out.append(ws_sel);
	}
}

void DiffController::applySelections()
{
	QList<QTextEdit::ExtraSelection> primarySel;
	QList<QTextEdit::ExtraSelection> secondarySel;
	QHash<int, CodeEditor::LineMark> primaryMarks;
	QHash<int, CodeEditor::LineMark> secondaryMarks;

	const QVector<AlignedLine> &lines = m_model.alignedLines();
	for (int i = 0; i < lines.size(); ++i) {
		const AlignedLine &al = lines[i];
		if (al.op == DiffOp::Equal)
			continue;

		const bool isCurrent = (al.hunkIndex >= 0 && al.hunkIndex == m_currentHunk);
		const CodeEditor::LineMark mark = isCurrent
			? CodeEditor::LineMark::Current
			: CodeEditor::LineMark::Diff;
		const QColor bg = isCurrent ? DiffColors::current() : DiffColors::diff();

		// Primary: строка есть при Delete/Replace, заполнитель при Insert.
		applyPanelSelection(m_primary, i, al, /*primarySide=*/true,
			bg, mark, primarySel, primaryMarks);

		// Secondary: строка есть при Insert/Replace, заполнитель при Delete.
		applyPanelSelection(m_secondary, i, al, /*primarySide=*/false,
			bg, mark, secondarySel, secondaryMarks);
	}

	m_primary->setBaseExtraSelections(primarySel);
	m_secondary->setBaseExtraSelections(secondarySel);
	m_primary->setLineMarks(primaryMarks);
	m_secondary->setLineMarks(secondaryMarks);
}

// ---------------------------------------------------------------- read-only

void DiffController::setEditorsReadOnly(bool readOnly)
{
    m_primary->setReadOnly(readOnly);
    m_secondary->setReadOnly(true);   // secondary всегда read-only в diff-режиме
}

// ---------------------------------------------------------------- navigation

bool DiffController::canGoPrevHunk() const
{
    return m_currentHunk > 0;
}

bool DiffController::canGoNextHunk() const
{
    return m_currentHunk >= 0 && m_currentHunk < m_model.hunkCount() - 1;
}

void DiffController::goPrevHunk()
{
    if (!canGoPrevHunk()) return;
    setCurrentHunk(m_currentHunk - 1, true);
}

void DiffController::goNextHunk()
{
    if (!canGoNextHunk()) return;
    setCurrentHunk(m_currentHunk + 1, true);
}

void DiffController::setCurrentHunk(int idx, bool scroll)
{
    if (idx < 0 || idx >= m_model.hunkCount()) {
        m_currentHunk = -1;
        applySelections();
        emit hunksChanged();
        return;
    }

    m_currentHunk = idx;
    
    if (scroll) {
        const DiffHunk &h = m_model.hunks()[idx];
        const int editorLine = h.firstAlignedLine;

        // Прокрутить оба редактора к началу hunka.
        QTextBlock pb = m_primary->document()->findBlockByNumber(editorLine);
        QTextBlock sb = m_secondary->document()->findBlockByNumber(editorLine);
        if (pb.isValid()) {
            QTextCursor c(pb);
            m_primary->setTextCursor(c);
            m_primary->ensureCursorVisible();
        }
        if (sb.isValid()) {
            QTextCursor c(sb);
            m_secondary->setTextCursor(c);
            m_secondary->ensureCursorVisible();
        }

        // После ensureCursorVisible QPlainTextEdit сам синхронизирует
        // скроллбар; но чтобы гарантировать одинаковую позицию, выровняем.
        if (!m_syncingScroll) {
            m_syncingScroll = true;
            m_secondary->verticalScrollBar()->setValue(
                m_primary->verticalScrollBar()->value());
            m_syncingScroll = false;
        }
    }

	applySelections();

    emit hunksChanged();
}

int DiffController::hunkNearCursor() const
{
    if (m_model.hunkCount() == 0)
        return -1;

    const int cursorLine = m_secondary->textCursor().blockNumber();

    // Найти hunk, в котором курсор, или ближайший следующий, или последний.
    for (int i = 0; i < m_model.hunkCount(); ++i) {
        const DiffHunk &h = m_model.hunks()[i];
        if (cursorLine >= h.firstAlignedLine
            && cursorLine < h.firstAlignedLine + h.lineCount)
            return i;
    }
    // Ближайший следующий
    for (int i = 0; i < m_model.hunkCount(); ++i) {
        if (m_model.hunks()[i].firstAlignedLine > cursorLine)
            return i;
    }
    return m_model.hunkCount() - 1;
}

// ---------------------------------------------------------------- apply

void DiffController::applyAlignedLineToPrimary(int alignedIndex)
{
    const AlignedLine &al = m_model.alignedLines()[alignedIndex];

    // Позиция в оригинальном (не раздутом) primary.
    // Проще: пересобрать m_primaryText из модели, применив только эту
    // строку. Но это O(N) на каждую строку. Для маленькой утилиты — ок.
    //
    // Подход: идём по alignedLines и строим новый текст:
    //   - Equal / Delete: берём primaryText (Delete потом отфильтруем)
    //   - Insert: если alignedIndex == этот индекс, вставляем secondaryText
    //   - Replace: если alignedIndex == этот индекс, вставляем secondaryText,
    //              иначе primaryText.
    // Delete на позиции alignedIndex — не добавляем primaryText.
    QStringList out;
    const QVector<AlignedLine> &lines = m_model.alignedLines();
    for (int i = 0; i < lines.size(); ++i) {
        const AlignedLine &l = lines[i];
        switch (l.op) {
        case DiffOp::Equal:
            out.append(l.primaryText);
            break;
        case DiffOp::Delete:
            if (i != alignedIndex)
                out.append(l.primaryText);
            // если i == alignedIndex — удаляем строку
            break;
        case DiffOp::Insert:
            if (i == alignedIndex)
                out.append(l.secondaryText);
            break;
        case DiffOp::Replace:
            if (i == alignedIndex)
                out.append(l.secondaryText);
            else
                out.append(l.primaryText);
            break;
        }
    }
    m_primaryText = out.join(QLatin1Char('\n'));
}

bool DiffController::applyHunk(int idx)
{
	if (idx < 0)
		idx = (m_currentHunk >= 0) ? m_currentHunk : hunkNearCursor();
	if (idx < 0 || idx >= m_model.hunkCount())
		return false;

	const DiffHunk &h = m_model.hunks()[idx];
	if (!h.hasInserts && !h.hasDeletes)
		return false;

	// --- ЯКОРЬ: первая реальная primary-строка ПОСЛЕ hunka ---
	// Считаем ДО recompute, по текущему выравниванию.
	const int lastAlignedOfHunk = h.firstAlignedLine + h.lineCount - 1;
	const int anchorPrimaryFileLine =
		firstPrimaryLineAfterAligned(lastAlignedOfHunk);

	// --- построение нового m_primaryText (как было) ---
	const QVector<AlignedLine> &lines = m_model.alignedLines();
	QStringList out;
	for (int i = 0; i < lines.size(); ++i) {
		const AlignedLine &l = lines[i];
		const bool inHunk = (l.hunkIndex == idx);

		switch (l.op) {
		case DiffOp::Equal:
			out.append(l.primaryText);
			break;
		case DiffOp::Delete:
			if (!inHunk)
				out.append(l.primaryText);
			break;
		case DiffOp::Insert:
			if (inHunk)
				out.append(l.secondaryText);
			break;
		case DiffOp::Replace:
			out.append(inHunk ? l.secondaryText : l.primaryText);
			break;
		}
	}
	m_primaryText = out.join(QLatin1Char('\n'));

	// --- пересчёт и выбор следующего hunka ---
	m_model.compute(m_primaryText, m_secondaryText, m_ignoreWhitespace);
	m_currentHunk = -1;
	loadAlignedTexts();

	const int next = nextHunkAfterPrimaryLine(anchorPrimaryFileLine);
	if (next >= 0)
		setCurrentHunk(next, /*scroll*/ true);

	m_primary->document()->setModified(true);
	emit primaryModifiedByDiff();
	emit hunksChanged();
	return true;
}

bool DiffController::applyLineAtSecondaryEditorLine(int secondaryEditorLine)
{
	if (secondaryEditorLine < 0
		|| secondaryEditorLine >= m_model.alignedCount())
		return false;

	const AlignedLine &al = m_model.alignedLines()[secondaryEditorLine];
	if (al.op == DiffOp::Equal || al.op == DiffOp::Delete)
		return false;   // нечего переносить

	// --- ЯКОРЬ: первая реальная primary-строка ПОСЛЕ этой aligned-строки ---
	// Считаем ДО applyAlignedLineToPrimary/recompute, по текущему выравниванию.
	const int anchorPrimaryFileLine =
		firstPrimaryLineAfterAligned(secondaryEditorLine);

	applyAlignedLineToPrimary(secondaryEditorLine);

	m_model.compute(m_primaryText, m_secondaryText, m_ignoreWhitespace);
	m_currentHunk = -1;
	loadAlignedTexts();

	const int next = nextHunkAfterPrimaryLine(anchorPrimaryFileLine);
	if (next >= 0)
		setCurrentHunk(next, /*scroll*/ true);

	m_primary->document()->setModified(true);
	emit primaryModifiedByDiff();
	emit hunksChanged();
	return true;
}
int DiffController::applyAllRemaining()
{
    if (m_model.hunkCount() == 0)
        return 0;

    const QVector<AlignedLine> &lines = m_model.alignedLines();
    QStringList out;
    out.reserve(lines.size());
    for (const AlignedLine &l : lines) {
        switch (l.op) {
        case DiffOp::Equal:
            out.append(l.primaryText);
            break;
        case DiffOp::Delete:
            // удаляем
            break;
        case DiffOp::Insert:
        case DiffOp::Replace:
            out.append(l.secondaryText);
            break;
        }
    }
    m_primaryText = out.join(QLatin1Char('\n'));

    const int applied = m_model.hunkCount();
    m_model.compute(m_primaryText, m_secondaryText, m_ignoreWhitespace);
    m_currentHunk = -1;
    loadAlignedTexts();

    m_primary->document()->setModified(true);
    emit primaryModifiedByDiff();
    emit hunksChanged();
    return applied;
}

// ---------------------------------------------------------------- scroll sync

void DiffController::connectScrollSync()
{
    connect(m_primary->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int) { syncScrollFrom(m_primary); });
    connect(m_secondary->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int) { syncScrollFrom(m_secondary); });
}

void DiffController::syncScrollFrom(CodeEditor *source)
{
    if (m_syncingScroll)
        return;
    m_syncingScroll = true;

    CodeEditor *target = (source == m_primary) ? m_secondary : m_primary;
    target->verticalScrollBar()->setValue(
        source->verticalScrollBar()->value());

    m_syncingScroll = false;
}

int DiffController::firstPrimaryLineAfterAligned(int alignedIndexExclusive) const
{
	const QVector<AlignedLine> &lines = m_model.alignedLines();
	for (int i = alignedIndexExclusive + 1; i < lines.size(); ++i) {
		const AlignedLine &al = lines[i];
		if (al.op != DiffOp::Insert && al.primaryFileLine >= 0)
			return al.primaryFileLine;
	}
	return std::numeric_limits<int>::max();
}

// Возвращает индекс hunka, который идёт первым после primary-строки anchor
// (или последний, если таковых нет; -1, если hunks нет).
int DiffController::nextHunkAfterPrimaryLine(int anchorPrimaryFileLine) const
{
	const int n = m_model.hunkCount();
	if (n == 0) 
		return -1;

	for (int i = 0; i < n; ++i) {
		const DiffHunk &nh = m_model.hunks()[i];
		int firstPrimary = std::numeric_limits<int>::max();
		for (int k = nh.firstAlignedLine;
			k < nh.firstAlignedLine + nh.lineCount; ++k) {
			const AlignedLine &al = m_model.alignedLines()[k];
			if (al.op != DiffOp::Insert && al.primaryFileLine >= 0) {
				firstPrimary = al.primaryFileLine;
				break;
			}
		}
		if (firstPrimary >= anchorPrimaryFileLine)
			return i;
	}
	// Все hunks оказались выше якоря (приняли последний) — берём последний.
	return n - 1;
}
