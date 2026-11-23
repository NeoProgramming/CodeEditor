#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "DiffModel.h"
#include "CodeEditor.h"

class CodeEditor;

// Связывает DiffModel с двумя CodeEditor'ами: подсветка, навигация,
// перенос hunks'ов. Создаётся EditorPane'ом на время diff-режима и
// уничтожается при выходе из него. Primary и secondary на время работы
// контроллера — read-only.
class DiffController : public QObject
{
    Q_OBJECT

public:
    DiffController(CodeEditor *primary, CodeEditor *secondary,
                   QObject *parent = nullptr);

    // Пересчитать diff по текущим «реальным» текстам primary/secondary.
    // Вызывается при создании и после каждого Apply.
    void recompute();

    bool ignoreWhitespace() const { return m_ignoreWhitespace; }
    void setIgnoreWhitespace(bool ignore);

    // ---- Навигация ----
    bool canGoPrevHunk() const;
    bool canGoNextHunk() const;
    void goPrevHunk();
    void goNextHunk();

    // Индекс текущего hunka (-1, если нет).
    int currentHunkIndex() const { return m_currentHunk; }

    // ---- Перенос ----
    // Применить hunk с индексом idx (или текущий, если idx < 0).
    // Возвращает true, если что-то было применено.
    bool applyHunk(int idx = -1);

    // Применить одну выровненную строку (editor line в secondary).
    // Если строка — Insert/Replace, она переносится в primary.
    // Если Delete — нечего переносить (false).
    bool applyLineAtSecondaryEditorLine(int secondaryEditorLine);

    // Применить все оставшиеся hunks (для Accept).
    // Возвращает количество применённых hunks.
    int applyAllRemaining();

    // ---- Доступ к модели (для UI, если понадобится) ----
    const DiffModel &model() const { return m_model; }

    // Реальные (не раздутые) тексты primary и secondary.
    // После apply* они синхронизированы с m_primaryText/m_secondaryText.
    QString primaryRealText()   const { return m_primaryText; }
    QString secondaryRealText() const { return m_secondaryText; }

signals:
    // Изменилось число/состав hunks'ов или текущий hunk — UI обновляет
    // enabled-состояние кнопок.
    void hunksChanged();

    // Был применён хотя бы один hunk — primary «грязный».
    void primaryModifiedByDiff();

private:
	void applyPanelSelection(
		CodeEditor *editor,
		int alignedIndex,
		const AlignedLine &al,		
		bool primarySide,
		const QColor &background,
		CodeEditor::LineMark mark,
		QList<QTextEdit::ExtraSelection> &out,
		QHash<int, CodeEditor::LineMark> &marks);	

    // Загрузить «раздутые» тексты в редакторы и повесить ExtraSelection.
    void loadAlignedTexts();

    // Перерисовать ExtraSelection (подсветка всех hunks + текущего).
    void applySelections();

    // Отключить/включить highlightCurrentLine у редакторов.
    void setEditorsReadOnly(bool readOnly);

    // Установить текущий hunk по индексу и проскроллить оба редактора.
    void setCurrentHunk(int idx, bool scroll = true);

    // Найти hunk, ближайший к позиции курсора secondary (для «текущего»).
    int hunkNearCursor() const;

    // Синхронная прокрутка: по номеру строки (тексты выровнены).
    void syncScrollFrom(CodeEditor *source);
    void connectScrollSync();

    // Применить один AlignedLine к m_primaryText.
    // Для Insert — вставить secondaryText на позицию.
    // Для Replace — заменить primaryText на secondaryText.
    // Для Delete — удалить primaryText (используется только applyHunk,
    // потому что отдельная строка Delete нечего «переносить»).
    void applyAlignedLineToPrimary(int alignedIndex);

    // Пересобрать m_primaryText/m_secondaryText из модели + применить
    // оставшиеся hunks (для applyAllRemaining).
    void rebuildTextsFromModel(bool dropAllHunks);

    // Восстановить позицию курсора в secondary после перезагрузки текстов.
    void restoreSecondaryCursor(int editorLine, int column);

    // Блокировка рекурсии в syncScrollFrom.
    bool m_syncingScroll = false;

    CodeEditor *m_primary = nullptr;
    CodeEditor *m_secondary = nullptr;

    DiffModel m_model;
    bool      m_ignoreWhitespace = true;

    // Реальные тексты (без заполнителей). Источник истины для модели.
    QString m_primaryText;
    QString m_secondaryText;

    int m_currentHunk = -1;

    // Чтобы временно отключить highlightCurrentLine в редакторах.
    // (CodeEditor::highlightCurrentLine вызывается по сигналу
    // cursorPositionChanged — его не «выключить» флагом, но можно
    // перезаписывать ExtraSelections после него. Здесь держим флаг,
    // чтобы applySelections знал, что рисовать.)
    bool m_highlightCurrentLineWasEnabled = true;
};
