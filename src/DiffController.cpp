#include "DiffController.h"
#include "CodeEditor.h"

#include <QTextCursor>
#include <QTextEdit>
#include <QTextBlock>
#include <QScrollBar>
#include <QTimer>
#include <QColor>
#include <QDebug>

// ---------------------------------------------------------------- colors
//
// Полупрозрачные — работают и на светлой, и на тёмной теме.
// Primary: удалённые/изменённые-старые — красноватые.
// Secondary: добавленные/изменённые-новые — зеленоватые.
// Текущий hunk — чуть ярче.

namespace {

QColor colorPrimaryHunk()      { return QColor(255, 80, 80, 50); }
QColor colorSecondaryHunk()    { return QColor(80, 200, 80, 50); }
QColor colorPrimaryCurrent()   { return QColor(255, 80, 80, 110); }
QColor colorSecondaryCurrent() { return QColor(80, 200, 80, 110); }
QColor colorPrimaryWord()      { return QColor(255, 0, 0, 120); }
QColor colorSecondaryWord()    { return QColor(0, 160, 0, 120); }
QColor colorFiller()           { return QColor(0, 0, 0, 20); }   // пустые строки-заполнители

} // namespace

// ---------------------------------------------------------------- ctor

DiffController::DiffController(CodeEditor *primary, CodeEditor *secondary,
                               QObject *parent)
    : QObject(parent)
    , m_primary(primary)
    , m_secondary(secondary)
{
    Q_ASSERT(m_primary && m_secondary);

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

        // ---- Primary ----
        if (al.op == DiffOp::Delete || al.op == DiffOp::Replace) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(isCurrent ? colorPrimaryCurrent()
                                               : colorPrimaryHunk());
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            QTextCursor c(m_primary->document()->findBlockByNumber(i));
            c.clearSelection();
            sel.cursor = c;
            primarySel.append(sel);

			primaryMarks.insert(i, isCurrent ? CodeEditor::LineMark::Current
				: CodeEditor::LineMark::Delete);

            // Пословная подсветка для Replace — поверх фоновой.
            if (al.op == DiffOp::Replace) {
                const WordDiff wd = m_model.wordDiffFor(i);
                QTextBlock block = m_primary->document()->findBlockByNumber(i);
                for (const WordSpan &ws : wd.primaryChanged) {
                    QTextEdit::ExtraSelection ws_sel;
                    ws_sel.format.setBackground(colorPrimaryWord());
                    QTextCursor wc(block);
                    wc.setPosition(block.position() + ws.start);
                    wc.setPosition(block.position() + ws.start + ws.length,
                                   QTextCursor::KeepAnchor);
                    ws_sel.cursor = wc;
                    primarySel.append(ws_sel);
                }
            }
        }
        else if (al.op == DiffOp::Insert) {
            // Строка-заполнитель в primary — отметим лёгким фоном,
            // чтобы визуально отличать от настоящей пустой строки.
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(colorFiller());
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            QTextCursor c(m_primary->document()->findBlockByNumber(i));
            c.clearSelection();
            sel.cursor = c;
            primarySel.append(sel);

			primaryMarks.insert(i, CodeEditor::LineMark::None);   // заполнитель — без метки
        }

        // ---- Secondary ----
        if (al.op == DiffOp::Insert || al.op == DiffOp::Replace) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(isCurrent ? colorSecondaryCurrent()
                                               : colorSecondaryHunk());
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            QTextCursor c(m_secondary->document()->findBlockByNumber(i));
            c.clearSelection();
            sel.cursor = c;
            secondarySel.append(sel);

            if (al.op == DiffOp::Replace) {
                const WordDiff wd = m_model.wordDiffFor(i);
                QTextBlock block = m_secondary->document()->findBlockByNumber(i);
                for (const WordSpan &ws : wd.secondaryChanged) {
                    QTextEdit::ExtraSelection ws_sel;
                    ws_sel.format.setBackground(colorSecondaryWord());
                    QTextCursor wc(block);
                    wc.setPosition(block.position() + ws.start);
                    wc.setPosition(block.position() + ws.start + ws.length,
                                   QTextCursor::KeepAnchor);
                    ws_sel.cursor = wc;
                    secondarySel.append(ws_sel);

					secondaryMarks.insert(i, isCurrent ? CodeEditor::LineMark::Current
						: CodeEditor::LineMark::Insert);
                }
            }
        }
        else if (al.op == DiffOp::Delete) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(colorFiller());
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            QTextCursor c(m_secondary->document()->findBlockByNumber(i));
            c.clearSelection();
            sel.cursor = c;
            secondarySel.append(sel);

			secondaryMarks.insert(i, CodeEditor::LineMark::None);
        }
    }

    m_primary->setExtraSelections(primarySel);
    m_secondary->setExtraSelections(secondarySel);
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

    // Применяем все строки hunka к m_primaryText.
    // Идём от конца к началу, чтобы индексы не «плыли» при удалении.
    // Но проще — пересобрать m_primaryText из модели с учётом
    // применения всех строк hunka. Сделаем через построение.
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

    // Запомним, куда вернуть курсор secondary: на первую строку hunka.
    const int restoreLine = h.firstAlignedLine;

    // Пересчитать модель и перезагрузить редакторы.
    m_model.compute(m_primaryText, m_secondaryText, m_ignoreWhitespace);
    m_currentHunk = -1;
    loadAlignedTexts();
    if (m_model.hunkCount() > 0)
        setCurrentHunk(qMin(restoreLine, m_model.hunkCount() - 1),
                       /*scroll*/ true);

    // Пометить primary как изменённый.
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

    applyAlignedLineToPrimary(secondaryEditorLine);

    const int restoreLine = secondaryEditorLine;
    m_model.compute(m_primaryText, m_secondaryText, m_ignoreWhitespace);
    m_currentHunk = -1;
    loadAlignedTexts();
    if (m_model.hunkCount() > 0)
        setCurrentHunk(qMin(restoreLine, m_model.hunkCount() - 1), true);

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
