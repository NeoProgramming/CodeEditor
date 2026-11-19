#pragma once

#include <QMainWindow>
#include "syntaxstyle.h"

class QAction;
class QTabWidget;
class QMenu;
class QToolBar;
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
	void openFolder();
	void openFileByPath(const QString &path);
	bool saveFile();
	bool saveFileAs();
	void closeCurrentTab();
	void closeTab(int index);
	void onTabChanged(int index);
	void updateWindowTitle();


	void chooseFont();
	void toggleDarkTheme(bool enabled);
	void about();
	
	// ---- Versions ----
	void onCreateVersion();
	void onAcceptSecondary();
	void onDeleteVersion();
	void onDeleteAllVersionsThisFile();
	void onDeleteAllVersionsAllFiles();
	void onPrevVersion();
	void onNextVersion();
	void onCloseVersion();
	void onShowBuffer();
	void onPasteFromClipboard();
	void updateVersionActions();

	// ---- Diff (заготовки, реализация позже) ----
	void onPrevDifference();
	void onNextDifference();
	void onApplyDifference();

	void onReloadWithEncoding();
	void onSaveWithEncoding();

private:
	void createActions();
	void createMenus();
	void createToolBars();
	void createViewMenu();
	void createStatusBar();

	void applyActionPresentation(QAction *act,
		const QString &iconPath,
		const QString &toolTip);
	void applyToolButtonStyle(QToolBar *bar, QAction *act);

	EditorPane *currentPane() const;
	EditorPane *createPane();
	void        addPaneTab(EditorPane *pane);
	void        connectPane(EditorPane *pane);
	int         nextUntitledNumber();

	bool maybeSave(EditorPane *pane);   // true = можно закрывать
	void rebuildFontPool();

	QTabWidget  *m_tabs = nullptr;
	FontManager *m_fontManager = nullptr;
	FolderTree  *m_folderTree = nullptr;

	QToolBar *m_mainToolBar = nullptr;
	QToolBar *m_diffToolBar = nullptr;

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

	// Versions
	QAction *createVersionAct;
	QAction *acceptSecondaryAct;
	QAction *deleteVersionAct;
	QAction *deleteAllVersionsThisAct;
	QAction *deleteAllVersionsAllAct;
	QAction *prevVersionAct;
	QAction *nextVersionAct;
	QAction *closeVersionAct;
	QAction *showBufferAct;
	QAction *pasteClipboardAct;

	// Diff (заготовки; будут задействованы вместе с DiffController)
	QAction *prevDifferenceAct;
	QAction *nextDifferenceAct;
	QAction *applyDifferenceAct;

	QAction *reloadEncodingAct;
	QAction *saveEncodingAct;

	int m_untitledCounter = 0;
};
