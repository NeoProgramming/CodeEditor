#include "MainWindow.h"
#include "EditorPane.h"
#include "FontManager.h"
#include "FontDialog.h"
#include "codeeditor.h"
#include "FolderTree.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QDebug>

// ---------------------------------------------------------------- ctor

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	menuBar()->setNativeMenuBar(false);

	// 1. FontManager и стартовый набор стилей — общие на приложение
	m_fontManager = new FontManager(this);
	m_fontManager->setCellSize(8, 16);

	m_syntaxElements = defaultSyntaxElements();
	rebuildFontPool();

	// 2. Область вкладок
	m_tabs = new QTabWidget(this);
	m_tabs->setTabsClosable(true);
	m_tabs->setMovable(true);
	m_tabs->setDocumentMode(true);
	setCentralWidget(m_tabs);

	// Dock с деревом файлов
	m_folderTree = new FolderTree(this);
	addDockWidget(Qt::LeftDockWidgetArea, m_folderTree);
	connect(m_folderTree, &FolderTree::openFileRequested,
		this, &MainWindow::openFileByPath);

	connect(m_tabs, &QTabWidget::currentChanged,
		this, &MainWindow::onTabChanged);
	connect(m_tabs, &QTabWidget::tabCloseRequested,
		this, &MainWindow::closeTab);

	createActions();
	createMenus();
	createStatusBar();

	resize(1000, 700);

	newFile();   // стартовая пустая вкладка
}

// ---------------------------------------------------------------- actions / menus

void MainWindow::createActions()
{
	// ----- File -----
	newAct = new QAction("&New", this);
	newAct->setShortcut(QKeySequence::New);
	connect(newAct, &QAction::triggered, this, &MainWindow::newFile);

	openAct = new QAction("&Open...", this);
	openAct->setShortcut(QKeySequence::Open);
	connect(openAct, &QAction::triggered, this, &MainWindow::openFile);

	openFolderAct = new QAction("Open &Folder...", this);
	connect(openFolderAct, &QAction::triggered,
		this, &MainWindow::openFolder);

	saveAct = new QAction("&Save", this);
	saveAct->setShortcut(QKeySequence::Save);
	connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);

	saveAsAct = new QAction("Save &As...", this);
	saveAsAct->setShortcut(QKeySequence::SaveAs);
	connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveFileAs);

	closeAct = new QAction("&Close", this);
	closeAct->setShortcut(QKeySequence::Close);
	connect(closeAct, &QAction::triggered,
		this, &MainWindow::closeCurrentTab);

	quitAct = new QAction("&Quit", this);
	quitAct->setShortcut(QKeySequence::Quit);
	connect(quitAct, &QAction::triggered, this, &QWidget::close);

	// ----- Compare -----
	compareClipboardAct = new QAction("Compare with &Clipboard", this);
	connect(compareClipboardAct, &QAction::triggered,
		this, &MainWindow::compareWithClipboard);

	stopCompareAct = new QAction("Stop &Comparison", this);
	connect(stopCompareAct, &QAction::triggered,
		this, &MainWindow::stopComparison);

	// ----- Settings -----
	fontAct = new QAction("&Font...", this);
	connect(fontAct, &QAction::triggered, this, &MainWindow::chooseFont);

	darkThemeAct = new QAction("&Dark Theme", this);
	darkThemeAct->setCheckable(true);
	connect(darkThemeAct, &QAction::toggled,
		this, &MainWindow::toggleDarkTheme);

	// ----- Help -----
	aboutAct = new QAction("&About", this);
	connect(aboutAct, &QAction::triggered, this, &MainWindow::about);

	// ----- Edit shortcuts (без меню) -----
	auto *undoAct = new QAction("Undo", this);
	undoAct->setShortcut(QKeySequence::Undo);
	undoAct->setShortcutContext(Qt::ApplicationShortcut);
	connect(undoAct, &QAction::triggered,
		this, [this]() {
		if (auto *p = currentPane()) p->primaryEditor()->undo();
	});
	addAction(undoAct);

	auto *redoAct = new QAction("Redo", this);
	redoAct->setShortcut(QKeySequence::Redo);
	redoAct->setShortcutContext(Qt::ApplicationShortcut);
	connect(redoAct, &QAction::triggered,
		this, [this]() {
		if (auto *p = currentPane()) p->primaryEditor()->redo();
	});
	addAction(redoAct);

	auto *cutAct = new QAction("Cut", this);
	cutAct->setShortcut(QKeySequence::Cut);
	cutAct->setShortcutContext(Qt::ApplicationShortcut);
	connect(cutAct, &QAction::triggered,
		this, [this]() {
		if (auto *p = currentPane()) p->primaryEditor()->cut();
	});
	addAction(cutAct);

	auto *copyAct = new QAction("Copy", this);
	copyAct->setShortcut(QKeySequence::Copy);
	copyAct->setShortcutContext(Qt::ApplicationShortcut);
	connect(copyAct, &QAction::triggered,
		this, [this]() {
		if (auto *p = currentPane()) p->primaryEditor()->copy();
	});
	addAction(copyAct);

	auto *pasteAct = new QAction("Paste", this);
	pasteAct->setShortcut(QKeySequence::Paste);
	pasteAct->setShortcutContext(Qt::ApplicationShortcut);
	connect(pasteAct, &QAction::triggered,
		this, [this]() {
		if (auto *p = currentPane()) p->primaryEditor()->paste();
	});
	addAction(pasteAct);
}

void MainWindow::createMenus()
{
	QMenu *fileMenu = menuBar()->addMenu("&File");
	fileMenu->addAction(newAct);
	fileMenu->addAction(openAct);
	fileMenu->addAction(openFolderAct);
	fileMenu->addAction(saveAct);
	fileMenu->addAction(saveAsAct);
	fileMenu->addSeparator();
	fileMenu->addAction(closeAct);
	fileMenu->addAction(quitAct);

	QMenu *compareMenu = menuBar()->addMenu("&Compare");
	compareMenu->addAction(compareClipboardAct);
	compareMenu->addAction(stopCompareAct);

	QMenu *settingsMenu = menuBar()->addMenu("&Settings");
	settingsMenu->addAction(fontAct);
	settingsMenu->addAction(darkThemeAct);

	QMenu *helpMenu = menuBar()->addMenu("&Help");
	helpMenu->addAction(aboutAct);
}

void MainWindow::createStatusBar()
{
	statusBar()->showMessage("Ready");
}

// ---------------------------------------------------------------- panes

int MainWindow::nextUntitledNumber()
{
	return ++m_untitledCounter;
}

EditorPane *MainWindow::currentPane() const
{
	return qobject_cast<EditorPane *>(m_tabs->currentWidget());
}

EditorPane *MainWindow::createPane()
{
	auto *pane = new EditorPane(m_fontManager, m_syntaxElements,
		nextUntitledNumber(), m_tabs);
	connectPane(pane);
	return pane;
}

void MainWindow::connectPane(EditorPane *pane)
{
	connect(pane, &EditorPane::titleChanged,
		this, [this, pane]() {
		const int idx = m_tabs->indexOf(pane);
		if (idx >= 0)
			m_tabs->setTabText(idx, pane->tabTitle());
		if (pane == currentPane())
			updateWindowTitle();
	});

	connect(pane, &EditorPane::cursorPositionChanged,
		this, [this, pane](int line, int col) {
		if (pane != currentPane()) return;
		statusBar()->showMessage(
			QString("Line: %1, Column: %2").arg(line).arg(col));
	});
}

void MainWindow::addPaneTab(EditorPane *pane)
{
	const int idx = m_tabs->addTab(pane, pane->tabTitle());
	m_tabs->setCurrentIndex(idx);
}

void MainWindow::onTabChanged(int /*index*/)
{
	updateWindowTitle();
	if (auto *p = currentPane()) {
		const auto c = p->primaryEditor()->textCursor();
		statusBar()->showMessage(
			QString("Line: %1, Column: %2")
			.arg(c.blockNumber() + 1)
			.arg(c.positionInBlock() + 1));

		// Выделить файл в дереве, если он внутри корня
		if (m_folderTree)
			m_folderTree->selectFile(p->primaryState().filePath);
	}
}

void MainWindow::updateWindowTitle()
{
	EditorPane *pane = currentPane();
	QString title = "CodeEditor";
	if (pane) {
		title += " - " + pane->tabTitle();
	}
	setWindowTitle(title);
}

// ---------------------------------------------------------------- file ops

void MainWindow::newFile()
{
	addPaneTab(createPane());
}

void MainWindow::openFileByPath(const QString &path)
{
	if (path.isEmpty()) return;

	// Уже открыт?
	for (int i = 0; i < m_tabs->count(); ++i) {
		auto *p = qobject_cast<EditorPane *>(m_tabs->widget(i));
		if (p && p->primaryState().filePath == path) {
			m_tabs->setCurrentIndex(i);
			return;
		}
	}

	auto *pane = createPane();
	QString error;
	if (!pane->loadIntoPrimary(path, &error)) {
		QMessageBox::warning(this, "Error",
			"Cannot open file:\n" + error);
		pane->deleteLater();
		return;
	}
	addPaneTab(pane);
}

void MainWindow::openFile()
{
	const QString fileName = QFileDialog::getOpenFileName(
		this, "Open File",
		m_folderTree->rootPath().isEmpty()
		? QString()
		: m_folderTree->rootPath(),
		"All Files (*.*);;C++ (*.cpp *.h *.hpp);;Python (*.py)");
	if (fileName.isEmpty())
		return;
	openFileByPath(fileName);
}

void MainWindow::openFolder()
{
	const QString dir = QFileDialog::getExistingDirectory(
		this, "Open Folder",
		m_folderTree->rootPath().isEmpty()
		? QDir::homePath()
		: m_folderTree->rootPath());
	if (dir.isEmpty())
		return;
	m_folderTree->openFolder(dir);
}

bool MainWindow::saveFile()
{
	EditorPane *pane = currentPane();
	if (!pane) return false;

	if (pane->primaryState().filePath.isEmpty())
		return saveFileAs();

	QString error;
	if (!pane->savePrimary(&error)) {
		QMessageBox::warning(this, "Error",
			"Cannot save file:\n" + error);
		return false;
	}
	return true;
}

bool MainWindow::saveFileAs()
{
	EditorPane *pane = currentPane();
	if (!pane) return false;

	const QString fileName = QFileDialog::getSaveFileName(
		this, "Save As",
		pane->primaryState().filePath,
		"All Files (*.*)");
	if (fileName.isEmpty())
		return false;

	QString error;
	if (!pane->savePrimaryAs(fileName, &error)) {
		QMessageBox::warning(this, "Error",
			"Cannot save file:\n" + error);
		return false;
	}
	return true;
}

bool MainWindow::maybeSave(EditorPane *pane)
{
	if (!pane || !pane->primaryState().modified)
		return true;

	const auto btn = QMessageBox::warning(
		this, "Unsaved changes",
		QString("Document \"%1\" has unsaved changes.\nSave?")
		.arg(pane->primaryState().displayName()),
		QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

	if (btn == QMessageBox::Cancel)  return false;
	if (btn == QMessageBox::Discard) return true;

	// Save
	if (pane->primaryState().filePath.isEmpty()) {
		const QString fileName = QFileDialog::getSaveFileName(
			this, "Save As", QString(), "All Files (*.*)");
		if (fileName.isEmpty()) return false;
		QString error;
		return pane->savePrimaryAs(fileName, &error);
	}
	QString error;
	return pane->savePrimary(&error);
}

void MainWindow::closeTab(int index)
{
	auto *pane = qobject_cast<EditorPane *>(m_tabs->widget(index));
	if (!pane) return;
	if (!maybeSave(pane)) return;

	m_tabs->removeTab(index);
	pane->deleteLater();

	if (m_tabs->count() == 0)
		newFile();
}

void MainWindow::closeCurrentTab()
{
	closeTab(m_tabs->currentIndex());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	for (int i = 0; i < m_tabs->count(); ++i) {
		auto *pane = qobject_cast<EditorPane *>(m_tabs->widget(i));
		if (!maybeSave(pane)) {
			event->ignore();
			return;
		}
	}
	event->accept();
}

// ---------------------------------------------------------------- compare

void MainWindow::compareWithClipboard()
{
	EditorPane *pane = currentPane();
	if (!pane) return;

	const QString clip = QApplication::clipboard()->text();
	if (clip.isEmpty()) {
		statusBar()->showMessage("Clipboard is empty", 3000);
		return;
	}
	pane->setSecondary(SecondarySource::Clipboard, clip,
		QStringLiteral("clipboard"));
	statusBar()->showMessage("Comparing with clipboard", 3000);
}

void MainWindow::stopComparison()
{
	if (auto *pane = currentPane())
		pane->clearSecondary();
}

// ---------------------------------------------------------------- settings

void MainWindow::chooseFont()
{
	FontDialog dlg(m_fontManager, m_syntaxElements, this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	m_fontManager->setCellSize(dlg.cellWidth(), dlg.cellHeight());
	m_syntaxElements = dlg.elements();
	rebuildFontPool();

	// Применяем ко всем вкладкам, к обоим редакторам в каждой
	for (int i = 0; i < m_tabs->count(); ++i) {
		auto *pane = qobject_cast<EditorPane *>(m_tabs->widget(i));
		if (!pane) continue;
		pane->applySyntaxStyles(m_syntaxElements);
		pane->applyFontsChanged();
		pane->primaryEditor()->viewport()->update();
		if (pane->secondaryEditor())
			pane->secondaryEditor()->viewport()->update();
	}
}

void MainWindow::toggleDarkTheme(bool enabled)
{
	if (enabled) {
		qApp->setStyleSheet(
			"QMainWindow, QPlainTextEdit, QMenuBar, QMenu, QStatusBar {"
			"   background-color: #2b2b2b;"
			"   color: #dcdcdc;"
			"}"
			"QPlainTextEdit {"
			"   selection-background-color: #444;"
			"}"
			"QMenuBar::item:selected, QMenu::item:selected {"
			"   background-color: #3c3c3c;"
			"}");
	}
	else {
		qApp->setStyleSheet(QString());
	}
}

void MainWindow::about()
{
	QMessageBox::about(this, "About",
		"<b>CodeEditor</b><br>"
		"A simple code editor built with Qt5 and QPlainTextEdit.<br>"
		"Demonstrates syntax highlighting, line numbers, "
		"tabbed multi-document editing, and side-by-side comparison.");
}

void MainWindow::rebuildFontPool()
{
	m_fontManager->clear();

	for (const SyntaxElementStyle &e : m_syntaxElements) {
		const int idx = m_fontManager->addFont(e.fontFamily, e.bold, e.italic);
		if (idx < 0) {
			qWarning() << "Cannot calibrate font for element:" << e.id
				<< "family:" << e.fontFamily;
		}
	}

	emit m_fontManager->fontsChanged();
}
