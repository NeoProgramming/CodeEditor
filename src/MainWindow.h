#pragma once

#include <QMainWindow>
#include "syntaxstyle.h"

class QAction;
class QTabWidget;
class FontManager;
class EditorPane;
class FolderTree;   // новый forward

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);

protected:
	void closeEvent(QCloseEvent *event) override;

private slots:
	void newFile();
	void openFile();
	bool saveFile();
	bool saveFileAs();
	void closeCurrentTab();
	void closeTab(int index);
	void onTabChanged(int index);
	void updateWindowTitle();

	void compareWithClipboard();
	void stopComparison();

	void chooseFont();
	void toggleDarkTheme(bool enabled);
	void about();

	void openFolder();
	void openFileByPath(const QString &path);   // переиспользуется из openFile и из дерева

private:
	void createActions();
	void createMenus();
	void createStatusBar();

	EditorPane *currentPane() const;
	EditorPane *createPane();
	void        addPaneTab(EditorPane *pane);
	void        connectPane(EditorPane *pane);
	int         nextUntitledNumber();

	bool maybeSave(EditorPane *pane);   // true = можно закрывать
	void rebuildFontPool();

	QTabWidget  *m_tabs = nullptr;
	FontManager *m_fontManager = nullptr;
	FolderTree *m_folderTree = nullptr;

	QVector<SyntaxElementStyle> m_syntaxElements;

	QAction *newAct;
	QAction *openAct;
	QAction *openFolderAct;
	QAction *saveAct;
	QAction *saveAsAct;
	QAction *closeAct;
	QAction *quitAct;
	QAction *compareClipboardAct;
	QAction *stopCompareAct;
	QAction *fontAct;
	QAction *darkThemeAct;
	QAction *aboutAct;

	int m_untitledCounter = 0;
};
