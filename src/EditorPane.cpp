#include "EditorPane.h"
#include "CodeEditor.h"
#include "FontManager.h"
#include "VersionStore.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QTextDocument>
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>
#include <QClipboard>

QString secondarySourceLabel(SecondarySource src)
{
    switch (src) {
	case SecondarySource::Version: return QStringLiteral("version");
	case SecondarySource::Current: return QStringLiteral("current");
	case SecondarySource::Buffer:  return QStringLiteral("buffer");
	case SecondarySource::None:    break;
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
	emit secondaryChanged();
}

void EditorPane::showVersion(const QString &versionPath)
{
	QFile file(versionPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QMessageBox::warning(this, "Error",
			QString("Cannot open version file:\n%1").arg(versionPath));
		return;
	}
	QTextStream in(&file);
	const QString text = in.readAll();

	if (!m_secondary) {
		const QTextCursor saved = m_primary->textCursor();
		m_secondary = new CodeEditor(m_fontManager, m_elements, m_splitter);
		m_secondary->setReadOnly(true);
		m_splitter->addWidget(m_secondary);
		m_splitter->setSizes({ 1, 1 });
		m_primary->setTextCursor(saved);
		m_primary->setFocus();
	}

	m_secondary->setPlainText(text);
	m_secondarySource = SecondarySource::Version;
	m_secondaryVersionPath = versionPath;
	updateTitle();
	emit secondaryChanged();
}

void EditorPane::showCurrent()
{
	if (!m_secondary)
		return;
	m_secondary->setPlainText(m_primary->toPlainText());
	m_secondarySource = SecondarySource::Current;
	m_secondaryVersionPath.clear();
	updateTitle();
	emit secondaryChanged();
}

void EditorPane::showBuffer()
{
	if (!m_hasBuffer)
		return;

	if (!m_secondary) {
		const QTextCursor saved = m_primary->textCursor();
		m_secondary = new CodeEditor(m_fontManager, m_elements, m_splitter);
		m_secondary->setReadOnly(true);
		m_splitter->addWidget(m_secondary);
		m_splitter->setSizes({ 1, 1 });
		m_primary->setTextCursor(saved);
		m_primary->setFocus();
	}

	m_secondary->setPlainText(m_bufferText);
	m_secondarySource = SecondarySource::Buffer;
	m_secondaryVersionPath.clear();
	updateTitle();
	emit secondaryChanged();
}

void EditorPane::pasteFromClipboard()
{
	const QString clip = QApplication::clipboard()->text();
	if (clip.isEmpty()) {
		QMessageBox::warning(this, "Paste",
			"Clipboard is empty.");
		return;
	}
	m_bufferText = clip;
	m_hasBuffer = true;
	showBuffer();
}

bool EditorPane::ensureSavedOrConfirm(QString *error)
{
	if (!m_primaryState.modified)
		return true;
	if (m_primaryState.filePath.isEmpty()) {
		// Untitled - сохранять некуда
		QMessageBox::warning(this, "Unsaved",
			"Save the file first.");
		return false;
	}

	const auto btn = QMessageBox::question(this, "Unsaved changes",
		"The working file has unsaved changes.\n"
		"Save them before continuing?\n\n"
		"Yes  - save, then continue\n"
		"No   - discard changes and continue\n"
		"Cancel - abort",
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

	if (btn == QMessageBox::Cancel)
		return false;
	if (btn == QMessageBox::No)
		return true;   // продолжаем, несохранённое отбросится

	return savePrimary(error);
}

bool EditorPane::createVersion(QString *error)
{
	if (m_primaryState.filePath.isEmpty()) {
		if (error) *error = "File is not saved.";
		return false;
	}
	if (m_primaryState.modified) {
		// Создавать версию надо из сохранённого состояния
		if (!ensureSavedOrConfirm(error))
			return false;
	}

	QString err;
	const QString path = VersionStore::create(m_primaryState.filePath, &err);
	if (path.isEmpty()) {
		if (error) *error = err;
		return false;
	}
	return true;
}

bool EditorPane::acceptSecondary(QString *error)
{
	if (!m_secondary)
		return false;

	if (!ensureSavedOrConfirm(error))
		return false;

	switch (m_secondarySource) {
	case SecondarySource::Version: {
		QString err;
		if (!VersionStore::accept(m_primaryState.filePath,
			m_secondaryVersionPath, &err)) {
			if (error) *error = err;
			return false;
		}
		// Перечитаем file.cpp с диска
		QString loadErr;
		if (!loadIntoPrimary(m_primaryState.filePath, &loadErr)) {
			if (error) *error = loadErr;
			return false;
		}
		clearSecondary();
		return true;
	}
	case SecondarySource::Buffer: {
		QString err;
		if (!VersionStore::writeCurrent(m_primaryState.filePath,
			m_secondary->toPlainText(), &err)) {
			if (error) *error = err;
			return false;
		}
		QString loadErr;
		if (!loadIntoPrimary(m_primaryState.filePath, &loadErr)) {
			if (error) *error = loadErr;
			return false;
		}
		clearSecondary();
		return true;
	}
	case SecondarySource::Current:
	case SecondarySource::None:
		return false;
	}
	return false;
}

bool EditorPane::deleteSecondaryVersion(QString *error)
{
	if (m_secondarySource != SecondarySource::Version
		|| m_secondaryVersionPath.isEmpty())
		return false;

	QString err;
	if (!VersionStore::remove(m_secondaryVersionPath, &err)) {
		if (error) *error = err;
		return false;
	}
	clearSecondary();
	return true;
}

int EditorPane::deleteAllVersions(QString *error)
{
	if (m_primaryState.filePath.isEmpty())
		return 0;

	QString err;
	const int n = VersionStore::removeAll(m_primaryState.filePath, &err);
	if (n == 0 && !err.isEmpty() && error)
		*error = err;

	if (m_secondarySource == SecondarySource::Version)
		clearSecondary();
	return n;
}

// ---------------------------------------------------------------- navigation


	// Возвращает индекс в списке versions, если path - одна из них; иначе -1.
	int indexOfVersion(const QStringList &versions, const QString &path)
	{
		if (path.isEmpty()) return -1;
		for (int i = 0; i < versions.size(); ++i) {
			if (versions[i] == path)
				return i;
		}
		return -1;
	}


bool EditorPane::canGoPrevious() const
{
	if (m_primaryState.filePath.isEmpty())
		return false;
	const QStringList v = VersionStore::list(m_primaryState.filePath);
	if (v.isEmpty())
		return false;

	if (!m_secondary)
		return true;   // с пустого -> самая свежая

	switch (m_secondarySource) {
	case SecondarySource::Current:
		return true;   // с current -> самая свежая
	case SecondarySource::Version: {
		const int idx = indexOfVersion(v, m_secondaryVersionPath);
		return idx > 0;
	}
	default:
		return false;
	}
}

bool EditorPane::canGoNext() const
{
	if (m_primaryState.filePath.isEmpty())
		return false;
	const QStringList v = VersionStore::list(m_primaryState.filePath);
	if (v.isEmpty())
		return false;

	if (!m_secondary)
		return false;

	switch (m_secondarySource) {
	case SecondarySource::Version: {
		const int idx = indexOfVersion(v, m_secondaryVersionPath);
		// если idx валиден и это не последняя - можно вперёд
		return idx >= 0 && idx < v.size() - 1;
	}
	default:
		return false;
	}
}

void EditorPane::goPreviousVersion()
{
	if (!canGoPrevious())
		return;

	const QStringList v = VersionStore::list(m_primaryState.filePath);

	if (!m_secondary || m_secondarySource == SecondarySource::Current) {
		// с пустого/current -> самая свежая
		showVersion(v.last());
		return;
	}
	if (m_secondarySource == SecondarySource::Version) {
		const int idx = indexOfVersion(v, m_secondaryVersionPath);
		if (idx > 0)
			showVersion(v[idx - 1]);
		else if (idx == 0 && m_secondarySource == SecondarySource::Version) {
			// уже на самой старой - ничего
		}
	}
}

void EditorPane::goNextVersion()
{
	const QStringList v = VersionStore::list(m_primaryState.filePath);

	if (!m_secondary)
		return;

	switch (m_secondarySource) {
	case SecondarySource::Version: {
		const int idx = indexOfVersion(v, m_secondaryVersionPath);
		if (idx < 0) return;
		if (idx < v.size() - 1) {
			showVersion(v[idx + 1]);
		}
		else {
			// самая свежая -> current
			showCurrent();
		}
		break;
	}
	default:
		break;
	}
}

void EditorPane::closeVersion()
{
	clearSecondary();
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
		switch (m_secondarySource) {
		case SecondarySource::Version: {
			// mtime из имени файла: ~YYYYMMDD_HHMMSS
			const QString vname = QFileInfo(m_secondaryVersionPath).fileName();
			const int tilde = vname.lastIndexOf("~");
			QString stamp = tilde >= 0 ? vname.mid(tilde + 1) : vname;
			title += QStringLiteral(" <-> v ") + stamp;
			break;
		}
		case SecondarySource::Current:
			title += QStringLiteral(" <-> current");
			break;
		case SecondarySource::Buffer:
			title += QStringLiteral(" <-> buffer");
			break;
		default:
			break;
		}
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
