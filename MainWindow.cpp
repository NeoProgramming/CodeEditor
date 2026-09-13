#include "mainwindow.h"
#include "codeeditor.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTextStream>


MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	menuBar()->setNativeMenuBar(false);

	editor = new CodeEditor(this);
	setCentralWidget(editor);
	setWindowTitle("CodeEditor — Untitled");
	resize(900, 600);

	

	createActions();
	createMenus();
	createStatusBar();

	// Обновление позиции курсора в статусбаре
	connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
		auto c = editor->textCursor();
		statusBar()->showMessage(
			QString("Строка: %1, Столбец: %2")
			.arg(c.blockNumber() + 1)
			.arg(c.positionInBlock() + 1));
	});
}

void MainWindow::createActions()
{
	// ----- File -----
	newAct = new QAction("&New", this);
	newAct->setShortcut(QKeySequence::New);
	connect(newAct, &QAction::triggered, this, &MainWindow::newFile);

	openAct = new QAction("&Open...", this);
	openAct->setShortcut(QKeySequence::Open);
	connect(openAct, &QAction::triggered, this, &MainWindow::openFile);

	saveAct = new QAction("&Save", this);
	saveAct->setShortcut(QKeySequence::Save);
	connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);

	saveAsAct = new QAction("Save &As...", this);
	saveAsAct->setShortcut(QKeySequence::SaveAs);
	connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveFileAs);

	quitAct = new QAction("&Quit", this);
	quitAct->setShortcut(QKeySequence::Quit);
	connect(quitAct, &QAction::triggered, this, &QWidget::close);

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
	connect(undoAct, &QAction::triggered, editor, &QPlainTextEdit::undo);
	addAction(undoAct);

	auto *redoAct = new QAction("Redo", this);
	redoAct->setShortcut(QKeySequence::Redo);
	redoAct->setShortcutContext(Qt::ApplicationShortcut);
	connect(redoAct, &QAction::triggered, editor, &QPlainTextEdit::redo);
	addAction(redoAct);

	auto *cutAct = new QAction("Cut", this);
	cutAct->setShortcut(QKeySequence::Cut);
	cutAct->setShortcutContext(Qt::ApplicationShortcut);
	connect(cutAct, &QAction::triggered, editor, &QPlainTextEdit::cut);
	addAction(cutAct);

	auto *copyAct = new QAction("Copy", this);
	copyAct->setShortcut(QKeySequence::Copy);
	copyAct->setShortcutContext(Qt::ApplicationShortcut);
	connect(copyAct, &QAction::triggered, editor, &QPlainTextEdit::copy);
	addAction(copyAct);

	auto *pasteAct = new QAction("Paste", this);
	pasteAct->setShortcut(QKeySequence::Paste);
	pasteAct->setShortcutContext(Qt::ApplicationShortcut);
	connect(pasteAct, &QAction::triggered, editor, &QPlainTextEdit::paste);
	addAction(pasteAct);
}

void MainWindow::createMenus() 
{
	QMenu *fileMenu = menuBar()->addMenu("&File");
	fileMenu->addAction(newAct);
	fileMenu->addAction(openAct);
	fileMenu->addAction(saveAct);
	fileMenu->addAction(saveAsAct);
	fileMenu->addSeparator();
	fileMenu->addAction(quitAct);

	QMenu *settingsMenu = menuBar()->addMenu("&Settings");
	settingsMenu->addAction(fontAct);
	settingsMenu->addAction(darkThemeAct);

	QMenu *helpMenu = menuBar()->addMenu("&Help");
	helpMenu->addAction(aboutAct);
}
void MainWindow::createStatusBar() 
{ 
	statusBar()->showMessage("Готово"); 
}

void MainWindow::newFile()
{
	editor->clear();
	setCurrentFile(QString());
}

void MainWindow::openFile()
{
	QString fileName = QFileDialog::getOpenFileName(
		this, "Open File", QString(),
		"All Files (*.*);;C++ (*.cpp *.h *.hpp);;Python (*.py)");

	if (fileName.isEmpty())
		return;

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QMessageBox::warning(this, "Error", "Cannot open file.");
		return;
	}

	QTextStream in(&file);
	editor->setPlainText(in.readAll());
	setCurrentFile(fileName);
}

bool MainWindow::saveFile()
{
	if (currentFile.isEmpty())
		return saveFileAs();

	QFile file(currentFile);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QMessageBox::warning(this, "Error", "Cannot save file.");
		return false;
	}

	QTextStream out(&file);
	out << editor->toPlainText();
	setCurrentFile(currentFile);
	return true;
}

bool MainWindow::saveFileAs()
{
	QString fileName = QFileDialog::getSaveFileName(
		this, "Save As", QString(), "All Files (*.*)");

	if (fileName.isEmpty())
		return false;

	currentFile = fileName;
	return saveFile();
}

void MainWindow::setCurrentFile(const QString &fileName)
{
	currentFile = fileName;
	QString shown = fileName.isEmpty()
		? "Untitled"
		: QFileInfo(fileName).fileName();
	setWindowTitle(QString("CodeEditor — %1").arg(shown));
}

void MainWindow::chooseFont()
{
	bool ok = false;
	QFont font = QFontDialog::getFont(&ok, editor->font(), this, "Choose Font");
	if (ok)
		editor->setFont(font);
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
		"and file handling.");
}
