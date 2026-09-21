#pragma once

#include <QMainWindow>
#include "syntaxstyle.h"

class QAction;
class CodeEditor;
class FontManager;
class Highlighter;

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

	// Пересобрать пул шрифтов из m_syntaxElements
	void rebuildFontPool();

	// Сопоставление "id синтаксического элемента" -> индекс шрифта в FontManager
	int fontIndexFor(const QString &elementId) const;

	CodeEditor   *editor = nullptr;
	FontManager  *m_fontManager = nullptr;
	Highlighter  *m_highlighter = nullptr;

	QString currentFile;

	// Текущее состояние стилей и соответствие "элемент -> шрифт"
	QVector<SyntaxElementStyle> m_syntaxElements;
	QHash<QString, int>         m_elementFontIndex; // id -> index в FontManager

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
