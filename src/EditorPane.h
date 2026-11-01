#pragma once

#include <QWidget>
#include "syntaxstyle.h"
#include "PrimaryState.h"

class QSplitter;
class CodeEditor;
class FontManager;

// Откуда взят текст для второго (дополнительного) редактора.
// Второй редактор всегда read-only и никогда не участвует в Save.
enum class SecondarySource
{
    None,            // второго редактора нет
    Clipboard,       // снимок буфера обмена
    RepositoryCopy,  // резервная копия из репозитория / VCS
    Snapshot,        // снимок текущего файла на момент открытия diff
    Other            // произвольный текст, вставленный программно
};

// Человекочитаемая подпись источника для заголовка вкладки.
QString secondarySourceLabel(SecondarySource src);


// Вкладка редактора: один CodeEditor (primary) либо два (primary + secondary)
// в QSplitter. Primary всегда отображает рабочий файл; secondary — read-only
// копию из внешнего источника. Save/SaveAs/maybeSave относятся только к primary.
class EditorPane : public QWidget
{
    Q_OBJECT

public:
    EditorPane(FontManager *fontManager,
               const QVector<SyntaxElementStyle> &elements,
               int untitledNumber,
               QWidget *parent = nullptr);

    // ---- Primary (есть всегда) ----
    CodeEditor   *primaryEditor() const { return m_primary; }
    PrimaryState &primaryState()        { return m_primaryState; }
    const PrimaryState &primaryState() const { return m_primaryState; }

    // ---- Secondary (опционально) ----
    CodeEditor      *secondaryEditor() const { return m_secondary; }
    SecondarySource  secondarySource() const { return m_secondarySource; }
    bool             isDiffMode()      const { return m_secondary != nullptr; }

    // Открыть/обновить второй редактор.
    // source == SecondarySource::None — закрыть diff-режим.
    void setSecondary(SecondarySource source,
                      const QString &text,
                      const QString &displayLabel = QString());
    void clearSecondary();

    // ---- I/O (только primary) ----
    bool loadIntoPrimary(const QString &path, QString *error = nullptr);
    bool savePrimary(QString *error = nullptr);                       // по текущему пути
    bool savePrimaryAs(const QString &path, QString *error = nullptr);

    // Заголовок вкладки. primary.displayName() + (" *") + (" ↔ <source>").
    QString tabTitle() const;

    // Обновления после FontDialog — на оба редактора.
    void applySyntaxStyles(const QVector<SyntaxElementStyle> &elements);
    void applyFontsChanged();

signals:
    void titleChanged();
    void cursorPositionChanged(int line, int col);   // только primary

private:
    void connectPrimary();
    void connectSecondary();
    void updateTitle();

    FontManager *m_fontManager = nullptr;
    QVector<SyntaxElementStyle> m_elements;

    QSplitter  *m_splitter  = nullptr;
    CodeEditor *m_primary   = nullptr;
    CodeEditor *m_secondary = nullptr;

    PrimaryState    m_primaryState;
    SecondarySource m_secondarySource = SecondarySource::None;
    QString         m_secondaryLabel;
};
