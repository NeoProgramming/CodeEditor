#include "MainWindow.h"
#include "EditorPane.h"
#include "FontManager.h"
#include "FontDialog.h"
#include "VersionStore.h"
#include "codeeditor.h"
#include "FolderTree.h"
#include "DiffController.h"
#include "TextCodecUtils.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMenuBar>
#include <QIcon>
#include <QToolBar>
#include <QToolButton>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QDebug>
#include <QInputDialog>
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

	connect(m_tabs, &QTabWidget::currentChanged,
		this, &MainWindow::onTabChanged);
	connect(m_tabs, &QTabWidget::tabCloseRequested,
		this, &MainWindow::closeTab);

	// Dock с деревом файлов
	m_folderTree = new FolderTree(this);
	addDockWidget(Qt::LeftDockWidgetArea, m_folderTree);
	connect(m_folderTree, &FolderTree::openFileRequested,
		this, &MainWindow::openFileByPath);
	

	createActions();
	createToolBars();
	createMenus();
	createViewMenu();
	createStatusBar();

	resize(1000, 700);

	newFile();   // стартовая пустая вкладка
	updateVersionActions();
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

	// ----- Versions -----
	createVersionAct = new QAction("Create Version", this);
	connect(createVersionAct, &QAction::triggered,
		this, &MainWindow::onCreateVersion);

	acceptSecondaryAct = new QAction("Accept as Working", this);
	connect(acceptSecondaryAct, &QAction::triggered,
		this, &MainWindow::onAcceptSecondary);

	deleteVersionAct = new QAction("Delete Version", this);
	connect(deleteVersionAct, &QAction::triggered,
		this, &MainWindow::onDeleteVersion);

	deleteAllVersionsThisAct = new QAction("Delete All Versions (This File)", this);
	connect(deleteAllVersionsThisAct, &QAction::triggered,
		this, &MainWindow::onDeleteAllVersionsThisFile);

	deleteAllVersionsAllAct = new QAction("Delete All Versions (All Files)", this);
	connect(deleteAllVersionsAllAct, &QAction::triggered,
		this, &MainWindow::onDeleteAllVersionsAllFiles);

	prevVersionAct = new QAction("Previous Version", this);
	connect(prevVersionAct, &QAction::triggered,
		this, &MainWindow::onPrevVersion);

	nextVersionAct = new QAction("Next Version", this);
	connect(nextVersionAct, &QAction::triggered,
		this, &MainWindow::onNextVersion);

	closeVersionAct = new QAction("Close right panel", this);
	connect(closeVersionAct, &QAction::triggered,
		this, &MainWindow::onCloseVersion);

	showBufferAct = new QAction("Show Buffer", this);
	connect(showBufferAct, &QAction::triggered,
		this, &MainWindow::onShowBuffer);

	pasteClipboardAct = new QAction("Paste from Clipboard", this);
	connect(pasteClipboardAct, &QAction::triggered,
		this, &MainWindow::onPasteFromClipboard);

	// ----- Compare -----
	compareClipboardAct = new QAction("Compare with &Clipboard", this);

	

	// ----- Diff (заготовки; реальная логика появится вместе с DiffController) -----
	prevDifferenceAct = new QAction("Previous Difference", this);
	prevDifferenceAct->setEnabled(false);
	connect(prevDifferenceAct, &QAction::triggered,
		this, &MainWindow::onPrevDifference);

	nextDifferenceAct = new QAction("Next Difference", this);
	nextDifferenceAct->setEnabled(false);
	connect(nextDifferenceAct, &QAction::triggered,
		this, &MainWindow::onNextDifference);

	applyDifferenceAct = new QAction("Apply This Difference", this);
	applyDifferenceAct->setEnabled(false);
	connect(applyDifferenceAct, &QAction::triggered,
		this, &MainWindow::onApplyDifference);
	
	stopCompareAct = new QAction("Stop &Comparison", this);

	reloadEncodingAct = new QAction("Reload with Encoding...", this);
	connect(reloadEncodingAct, &QAction::triggered,
		this, &MainWindow::onReloadWithEncoding);

	saveEncodingAct = new QAction("Save with Encoding...", this);
	connect(saveEncodingAct, &QAction::triggered,
		this, &MainWindow::onSaveWithEncoding);

	
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

void MainWindow::applyActionPresentation(QAction *act,
	const QString &iconPath,
	const QString &toolTip)
{
	if (!act) return;
	if (!iconPath.isEmpty())
		act->setIcon(QIcon(iconPath));   // пусто до появления ресурсов — это ок
	if (!toolTip.isEmpty()) {
		act->setToolTip(toolTip);
		act->setStatusTip(toolTip);
	}
	
}

void MainWindow::createToolBars()
{
	// ---- Основной тулбар ----
	m_mainToolBar = addToolBar("Main");
	m_mainToolBar->setObjectName("MainToolBar");
	m_mainToolBar->setMovable(false);
	m_mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	// Презентация действий: иконка + тултип. Иконки появятся с ресурсами;
	// до этого QIcon будет пустым, но код готов.
	applyActionPresentation(newAct, ":/icons/res/file-new.png", "New file");
	applyActionPresentation(openAct, ":/icons/res/file-open.png", "Open file");
	applyActionPresentation(saveAct, ":/icons/res/file-save.png", "Save");
	applyActionPresentation(createVersionAct, ":/icons/res/version-create.png", "Create a version of the working file");
	applyActionPresentation(prevVersionAct, ":/icons/res/version-prev.png", "Show previous version");
	applyActionPresentation(nextVersionAct, ":/icons/res/version-next.png", "Show next version");
	applyActionPresentation(showBufferAct, ":/icons/res/version-buffer.png", "Show buffer in the right panel");
	applyActionPresentation(pasteClipboardAct, ":/icons/res/version-paste.png", "Paste from clipboard into buffer");

	applyActionPresentation(acceptSecondaryAct, ":/icons/res/version-accept.png", "Accept the right panel as working");
	applyActionPresentation(closeVersionAct, ":/icons/res/version-close.png", "Close right panel");

	applyActionPresentation(prevDifferenceAct, ":/icons/res/diff-prev.png", "Previous difference");
	applyActionPresentation(nextDifferenceAct, ":/icons/res/diff-next.png", "Next difference");
	applyActionPresentation(applyDifferenceAct, ":/icons/res/diff-apply.png", "Apply this difference");

	// Группировка: File | Versions | Secondary | Accept | Settings
	m_mainToolBar->addAction(newAct);
	m_mainToolBar->addAction(openAct);
	m_mainToolBar->addAction(saveAct);
	m_mainToolBar->addSeparator();

	m_mainToolBar->addAction(createVersionAct);
	m_mainToolBar->addAction(prevVersionAct);
	m_mainToolBar->addAction(nextVersionAct);
	m_mainToolBar->addSeparator();

	m_mainToolBar->addAction(showBufferAct);
	m_mainToolBar->addAction(pasteClipboardAct);
	m_mainToolBar->addSeparator();

	m_mainToolBar->addAction(acceptSecondaryAct);
	m_mainToolBar->addAction(closeVersionAct);

	// ---- Diff-тулбар (скрыт, пока нет diff) ----
	m_diffToolBar = addToolBar("Diff");
	m_diffToolBar->setObjectName("DiffToolBar");
	m_diffToolBar->setMovable(false);
	m_diffToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	//m_diffToolBar->setVisible(true);

	

	m_diffToolBar->addAction(prevDifferenceAct);
	m_diffToolBar->addAction(nextDifferenceAct);
	m_diffToolBar->addAction(applyDifferenceAct);

	///
	applyToolButtonStyle(m_mainToolBar, newAct);
	applyToolButtonStyle(m_mainToolBar, openAct);
	applyToolButtonStyle(m_mainToolBar, saveAct);
	applyToolButtonStyle(m_mainToolBar, createVersionAct);
	applyToolButtonStyle(m_mainToolBar, prevVersionAct);
	applyToolButtonStyle(m_mainToolBar, nextVersionAct);
	applyToolButtonStyle(m_mainToolBar, showBufferAct);
	applyToolButtonStyle(m_mainToolBar, pasteClipboardAct);
	applyToolButtonStyle(m_mainToolBar, acceptSecondaryAct);
	applyToolButtonStyle(m_mainToolBar, closeVersionAct);

	applyToolButtonStyle(m_diffToolBar, prevDifferenceAct);
	applyToolButtonStyle(m_diffToolBar, nextDifferenceAct);
	applyToolButtonStyle(m_diffToolBar, applyDifferenceAct);


	auto style = QStringLiteral(R"(
    QToolButton {
        background: #e4e0d8;
        border-top: 2px solid #ffffff;
        border-left: 2px solid #ffffff;
        border-bottom: 2px solid #404040;
        border-right: 2px solid #404040;
        padding: 1px;
        margin: 0px;
    }
    QToolButton:hover { 
        background: #d0dce4; 
    }
    QToolButton:pressed {
        border-top: 2px solid #404040;
        border-left: 2px solid #404040;
        border-bottom: 2px solid #ffffff;
        border-right: 2px solid #ffffff;
        padding-top: 3px; padding-left: 3px;
        padding-bottom: 1px; padding-right: 1px;
    }
	)");
	m_mainToolBar->setStyleSheet(style);
	m_diffToolBar->setStyleSheet(style);
}

void MainWindow::createViewMenu()
{
	QMenu *viewMenu = menuBar()->addMenu("&View");

	// Explorer dock
	if (m_folderTree)
		viewMenu->addAction(m_folderTree->toggleViewAction());

	viewMenu->addSeparator();

	if (m_mainToolBar)
		viewMenu->addAction(m_mainToolBar->toggleViewAction());
	if (m_diffToolBar)
		viewMenu->addAction(m_diffToolBar->toggleViewAction());
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
	fileMenu->addAction(reloadEncodingAct);
	fileMenu->addAction(saveEncodingAct);
	fileMenu->addSeparator();
	fileMenu->addAction(closeAct);
	fileMenu->addAction(quitAct);
	

	QMenu *versionsMenu = menuBar()->addMenu("&Versions");
	versionsMenu->addAction(createVersionAct);
	versionsMenu->addSeparator();
	versionsMenu->addAction(prevVersionAct);
	versionsMenu->addAction(nextVersionAct);
	versionsMenu->addSeparator();
	versionsMenu->addAction(showBufferAct);
	versionsMenu->addAction(pasteClipboardAct);
	versionsMenu->addSeparator();
	versionsMenu->addAction(acceptSecondaryAct);
	versionsMenu->addSeparator();
	versionsMenu->addAction(deleteVersionAct);
	versionsMenu->addAction(deleteAllVersionsThisAct);
	versionsMenu->addAction(deleteAllVersionsAllAct);

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

	connect(pane, &EditorPane::secondaryChanged,
		this, [this, pane]() {
		if (pane == currentPane())
			updateVersionActions();
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

	updateVersionActions();
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

void MainWindow::updateVersionActions()
{
	EditorPane *pane = currentPane();
	const bool hasPane = pane != nullptr;
	const bool hasFile = hasPane && !pane->primaryState().filePath.isEmpty();
	const bool hasSecond = hasPane && pane->isDiffMode();
	DiffController *dc = pane ? pane->diffController() : nullptr;

	createVersionAct->setEnabled(hasFile);
	acceptSecondaryAct->setEnabled(hasSecond
		&& (pane->secondarySource() == SecondarySource::Version
			|| pane->secondarySource() == SecondarySource::Buffer));
	deleteVersionAct->setEnabled(hasSecond
		&& pane->secondarySource() == SecondarySource::Version);
	deleteAllVersionsThisAct->setEnabled(hasFile);
	deleteAllVersionsAllAct->setEnabled(m_folderTree
		&& !m_folderTree->rootPath().isEmpty());
	prevVersionAct->setEnabled(hasPane && pane->canGoPrevious());
	nextVersionAct->setEnabled(hasPane && pane->canGoNext());
	showBufferAct->setEnabled(hasPane);   // даже если буфера нет - no-op
	pasteClipboardAct->setEnabled(hasPane);

    // Пока secondary активен, кнопки видны, но disabled — как только
	prevDifferenceAct->setEnabled(dc && dc->canGoPrevHunk());
	nextDifferenceAct->setEnabled(dc && dc->canGoNextHunk());
	applyDifferenceAct->setEnabled(dc && dc->currentHunkIndex() >= 0);

	// Показывать diff-тулбар только когда есть secondary
//	if (m_diffToolBar)
//		m_diffToolBar->setVisible(hasSecond);
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

void MainWindow::onCreateVersion()
{
	EditorPane *pane = currentPane();
	if (!pane) return;
	QString err;
	if (!pane->createVersion(&err)) {
		if (!err.isEmpty())
			QMessageBox::warning(this, "Cannot create version", err);
	}
	updateVersionActions();
}

void MainWindow::onAcceptSecondary()
{
	EditorPane *pane = currentPane();
	if (!pane) return;
	QString err;
	if (!pane->acceptSecondary(&err)) {
		if (!err.isEmpty())
			QMessageBox::warning(this, "Cannot accept version", err);
	}
	updateVersionActions();
}

void MainWindow::onDeleteVersion()
{
	EditorPane *pane = currentPane();
	if (!pane) return;

	const auto btn = QMessageBox::question(this, "Delete version",
		"Delete the version shown in the right panel?",
		QMessageBox::Yes | QMessageBox::No);
	if (btn != QMessageBox::Yes) return;

	QString err;
	if (!pane->deleteSecondaryVersion(&err)) {
		if (!err.isEmpty())
			QMessageBox::warning(this, "Cannot delete version", err);
	}
	updateVersionActions();
}

void MainWindow::onDeleteAllVersionsThisFile()
{
	EditorPane *pane = currentPane();
	if (!pane) return;

	const auto btn = QMessageBox::question(this, "Delete all versions",
		QString("Delete all versions of \"%1\"?")
		.arg(pane->primaryState().displayName()),
		QMessageBox::Yes | QMessageBox::No);
	if (btn != QMessageBox::Yes) return;

	QString err;
	const int n = pane->deleteAllVersions(&err);
	if (!err.isEmpty())
		QMessageBox::warning(this, "Cannot delete versions", err);

	statusBar()->showMessage(
		QString("Deleted %1 version(s).").arg(n), 3000);
	updateVersionActions();
}

void MainWindow::onDeleteAllVersionsAllFiles()
{
	if (!m_folderTree || m_folderTree->rootPath().isEmpty())
		return;

	const auto btn = QMessageBox::question(this, "Delete all versions",
		"Delete ALL version files under:\n" + m_folderTree->rootPath() +
		"\n\nThis cannot be undone.",
		QMessageBox::Yes | QMessageBox::No);
	if (btn != QMessageBox::Yes) return;

	QString err;
	const int n = VersionStore::removeAllUnder(m_folderTree->rootPath(), &err);
	if (!err.isEmpty())
		QMessageBox::warning(this, "Cannot delete some versions", err);

	// Закрыть secondary во всех вкладках, где показана версия
	for (int i = 0; i < m_tabs->count(); ++i) {
		auto *p = qobject_cast<EditorPane *>(m_tabs->widget(i));
		if (!p) continue;
		if (p->secondarySource() == SecondarySource::Version)
			p->clearSecondary();
	}

	statusBar()->showMessage(
		QString("Deleted %1 version file(s).").arg(n), 3000);
	updateVersionActions();
}

void MainWindow::onPrevVersion()
{
	if (auto *p = currentPane()) p->goPreviousVersion();
	updateVersionActions();
}

void MainWindow::onNextVersion()
{
	if (auto *p = currentPane()) p->goNextVersion();
	updateVersionActions();
}

void MainWindow::onCloseVersion()
{
	if (auto *p = currentPane()) p->closeVersion();
	updateVersionActions();
}

void MainWindow::onShowBuffer()
{
	if (auto *p = currentPane()) p->showBuffer();
	updateVersionActions();
}

void MainWindow::onPasteFromClipboard()
{
	if (auto *p = currentPane()) p->pasteFromClipboard();
	updateVersionActions();
}


void MainWindow::onPrevDifference()
{
	if (auto *p = currentPane())
		if (auto *dc = p->diffController())
			dc->goPrevHunk();
	updateVersionActions();
}

void MainWindow::onNextDifference()
{
	if (auto *p = currentPane())
		if (auto *dc = p->diffController())
			dc->goNextHunk();
	updateVersionActions();
}

void MainWindow::onApplyDifference()
{
	if (auto *p = currentPane())
		if (auto *dc = p->diffController())
			dc->applyHunk();
	updateVersionActions();
}

void MainWindow::applyToolButtonStyle(QToolBar *bar, QAction *act)
{
	if (!act || !m_mainToolBar) return;
	auto *btn = qobject_cast<QToolButton*>(bar->widgetForAction(act));
	if (!btn) return;

	if (act->icon().isNull()) {
		// Иконки нет — показываем текст
		btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
	}
	else {
		// Иконка есть — только иконка
		btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
	}
}

void MainWindow::onReloadWithEncoding()
{
	EditorPane *pane = currentPane();
	if (!pane || pane->primaryState().filePath.isEmpty())
		return;

	const QList<QByteArray> codecs = TextCodecUtils::availableCodecNames();
	QStringList items;
	for (const QByteArray &c : codecs)
		items << QString::fromLatin1(c);

	const QByteArray current = pane->primaryState().codecName;
	int currentIdx = items.indexOf(QString::fromLatin1(current));

	bool ok = false;
	const QString chosen = QInputDialog::getItem(
		this, "Reload with Encoding",
		"Encoding:", items, currentIdx, false, &ok);
	if (!ok || chosen.isEmpty())
		return;

	QString err;
	if (!pane->reloadWithEncoding(chosen.toLatin1(), &err))
		QMessageBox::warning(this, "Error", err);
}

void MainWindow::onSaveWithEncoding()
{
	EditorPane *pane = currentPane();
	if (!pane || pane->primaryState().filePath.isEmpty())
		return;

	const QList<QByteArray> codecs = TextCodecUtils::availableCodecNames();
	QStringList items;
	for (const QByteArray &c : codecs)
		items << QString::fromLatin1(c);

	const QByteArray current = pane->primaryState().codecName;
	int currentIdx = items.indexOf(QString::fromLatin1(current));

	bool ok = false;
	const QString chosen = QInputDialog::getItem(
		this, "Save with Encoding",
		"Encoding:", items, currentIdx, false, &ok);
	if (!ok || chosen.isEmpty())
		return;

	QString err;
	if (!pane->saveWithEncoding(chosen.toLatin1(), &err))
		QMessageBox::warning(this, "Error", err);
}
