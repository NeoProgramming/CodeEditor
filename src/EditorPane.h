#pragma once
// main file
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
	None,       // второго редактора нет
	Version,    // показывает ~<timestamp> с диска
	Current,    // показывает file.cpp (рабочий файл) в памяти
	Buffer      // показывает вставленное из буфера (в памяти панели)
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


	// Показать версию с диска. filePath версии сохраняется в m_secondaryVersionPath.
	void showVersion(const QString &versionPath);

	// Показать текущий file.cpp в secondary (для "next за самую свежую").
	void showCurrent();

	// Показать буфер (в памяти панели). Если буфера нет - ничего не делает.
	void showBuffer();

	// Вставить из системного clipboard в буфер и показать его.
	void pasteFromClipboard();

	// Закрыть secondary.
	void clearSecondary();


	// ---- Версии (действия над файлами) ----

	// Создать версию текущего primary.
	bool createVersion(QString *error = nullptr);

	// Принять версию, показанную в secondary, как рабочую.
	// Если secondary показывает Buffer - записать buffer в file.cpp.
	// Если secondary показывает Version - выполнить rename.
	// Current/None - ничего не делает (false).
	bool acceptSecondary(QString *error = nullptr);

	// Удалить версию, показанную в secondary.
	bool deleteSecondaryVersion(QString *error = nullptr);

	// Удалить все версии file.cpp. Закрывает secondary, если оно показывает Version.
	int  deleteAllVersions(QString *error = nullptr);

	// Навигация по оси версий: [~старые ... ~свежие] -> Current.
	void goPreviousVersion();
	void goNextVersion();
	void closeVersion();
	bool canGoPrevious() const;
	bool canGoNext() const;

	// ---- I/O primary (без изменений) ----
	bool loadIntoPrimary(const QString &path, QString *error = nullptr);
	bool savePrimary(QString *error = nullptr);
	bool savePrimaryAs(const QString &path, QString *error = nullptr);



    // Открыть/обновить второй редактор.
    // source == SecondarySource::None — закрыть diff-режим.
    void setSecondary(SecondarySource source,
                      const QString &text,
                      const QString &displayLabel = QString());

	  

    // Заголовок вкладки. primary.displayName() + (" *") + (" ↔ <source>").
    QString tabTitle() const;

    // Обновления после FontDialog — на оба редактора.
    void applySyntaxStyles(const QVector<SyntaxElementStyle> &elements);
    void applyFontsChanged();

signals:
    void titleChanged();
    void cursorPositionChanged(int line, int col);   // только primary
	// Испускается при изменении состояния secondary (открыт/закрыт/сменил источник),
	// чтобы MainWindow мог обновить enabled-состояние QAction.
	void secondaryChanged();
private:
    void connectPrimary();
    void connectSecondary();
    void updateTitle();
	bool ensureSavedOrConfirm(QString *error);   // спросить Save/Discard/Cancel

    FontManager *m_fontManager = nullptr;
    QVector<SyntaxElementStyle> m_elements;

    QSplitter  *m_splitter  = nullptr;
    CodeEditor *m_primary   = nullptr;
    CodeEditor *m_secondary = nullptr;

    PrimaryState    m_primaryState;
    SecondarySource m_secondarySource = SecondarySource::None;
    QString         m_secondaryLabel;

	QString m_secondaryVersionPath;  // путь к ~T, если secondarySource == Version
	QString m_bufferText;            // вставленное из буфера, в памяти
	bool    m_hasBuffer = false;
};
