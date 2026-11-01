#include "EditorPane.h"
#include "CodeEditor.h"
#include "FontManager.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QTextDocument>

QString secondarySourceLabel(SecondarySource src)
{
    switch (src) {
    case SecondarySource::Clipboard:      return QStringLiteral("clipboard");
    case SecondarySource::RepositoryCopy: return QStringLiteral("repo copy");
    case SecondarySource::Snapshot:       return QStringLiteral("snapshot");
    case SecondarySource::Other:          return QStringLiteral("external");
    case SecondarySource::None:           break;
    }
    return QString();
}

// ---------------------------------------------------------------- ctor

EditorPane::EditorPane(FontManager *fontManager,
                       const QVector<SyntaxElementStyle> &elements,
                       int untitledNumber,
                       QWidget *parent)
    : QWidget(parent)
    , m_fontManager(fontManager)
    , m_elements(elements)
{
    m_primaryState.untitledNumber = untitledNumber;

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    lay->addWidget(m_splitter);

    m_primary = new CodeEditor(m_fontManager, m_elements, m_splitter);
    m_splitter->addWidget(m_primary);

    connectPrimary();
    updateTitle();
}

// ---------------------------------------------------------------- primary

void EditorPane::connectPrimary()
{
    connect(m_primary->document(), &QTextDocument::modificationChanged,
            this, [this](bool modified) {
        m_primaryState.modified = modified;
        updateTitle();
    });

    connect(m_primary, &QPlainTextEdit::cursorPositionChanged,
            this, [this]() {
        const auto c = m_primary->textCursor();
        emit cursorPositionChanged(c.blockNumber() + 1,
                                   c.positionInBlock() + 1);
    });
}

bool EditorPane::loadIntoPrimary(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QTextStream in(&file);
    m_primary->setPlainText(in.readAll());
    m_primaryState.filePath = path;
    m_primaryState.modified = false;
    m_primary->document()->setModified(false);
    updateTitle();
    return true;
}

bool EditorPane::savePrimary(QString *error)
{
    if (m_primaryState.filePath.isEmpty())
        return false;
    return savePrimaryAs(m_primaryState.filePath, error);
}

bool EditorPane::savePrimaryAs(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out << m_primary->toPlainText();
    m_primaryState.filePath = path;
    m_primaryState.modified = false;
    m_primary->document()->setModified(false);
    updateTitle();
    return true;
}

// ---------------------------------------------------------------- secondary

void EditorPane::setSecondary(SecondarySource source,
                              const QString &text,
                              const QString &displayLabel)
{
    if (source == SecondarySource::None) {
        clearSecondary();
        return;
    }

    // Добавляем второй редактор при первом вызове
    if (!m_secondary) {
        // Запомним фокус/курсор primary, чтобы не «прыгнул» после addWidget
        const QTextCursor savedCursor = m_primary->textCursor();

        m_secondary = new CodeEditor(m_fontManager, m_elements, m_splitter);
        m_secondary->setReadOnly(true);
        m_splitter->addWidget(m_secondary);
        m_splitter->setSizes({1, 1});

        connectSecondary();

        m_primary->setTextCursor(savedCursor);
        m_primary->setFocus();
    }

    m_secondary->setPlainText(text);
    m_secondarySource = source;
    m_secondaryLabel  = displayLabel;
    updateTitle();
}

void EditorPane::clearSecondary()
{
    if (!m_secondary)
        return;

    m_secondary->deleteLater();
    m_secondary = nullptr;
    m_secondarySource = SecondarySource::None;
    m_secondaryLabel.clear();
    updateTitle();
}

void EditorPane::connectSecondary()
{
    // Второй редактор read-only; никакой логики modified/save у него нет.
    // Здесь можно было бы подключить синхронную прокрутку, если понадобится.
}

// ---------------------------------------------------------------- title

void EditorPane::updateTitle()
{
    emit titleChanged();
}

QString EditorPane::tabTitle() const
{
    QString title = m_primaryState.displayName();
    if (m_primaryState.modified)
        title += QStringLiteral(" *");

    if (m_secondary) {
        const QString label = m_secondaryLabel.isEmpty()
            ? secondarySourceLabel(m_secondarySource)
            : m_secondaryLabel;
        title += QStringLiteral(" ↔ ") + label;
    }
    return title;
}

// ---------------------------------------------------------------- updates

void EditorPane::applySyntaxStyles(const QVector<SyntaxElementStyle> &elements)
{
    m_elements = elements;
    if (m_primary)   m_primary->setSyntaxStyles(m_elements);
    if (m_secondary) m_secondary->setSyntaxStyles(m_elements);
}

void EditorPane::applyFontsChanged()
{
    if (m_primary)   m_primary->onFontsChanged();
    if (m_secondary) m_secondary->onFontsChanged();
}
