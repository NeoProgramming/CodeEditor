#pragma once

#include <QMainWindow>

class CodeEditor;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);

private slots:
	void newFile();
	void openFile();
	bool saveFile();
	bool saveFileAs();
	void chooseFont();
	void toggleDarkTheme(bool enabled);
	void about();

private:
	void createActions();
	void createMenus();
	void createStatusBar();
	void setCurrentFile(const QString &fileName);

	CodeEditor *editor;
	QString currentFile;

	// Actions as members
	QAction *newAct;
	QAction *openAct;
	QAction *saveAct;
	QAction *saveAsAct;
	QAction *quitAct;
	QAction *fontAct;
	QAction *darkThemeAct;
	QAction *aboutAct;
};
