#pragma once

#include <QDockWidget>
#include <QStringList>
#include <QVector>
#include <QIcon>

class QTreeView;
class QStandardItemModel;
class QStandardItem;
class QPoint;

// Левая dock-панель с деревом файлов.
// Сканирует заданную корневую папку по списку расширений один раз
// (без real-time мониторинга). Двойной клик / Enter на файле эмитит
// openFileRequested(path); решение "новая вкладка или активировать" —
// забота MainWindow.
class FolderTree : public QDockWidget
{
    Q_OBJECT

public:
    explicit FolderTree(QWidget *parent = nullptr);

    // Задать список расширений (без точки, регистронезависимо).
    // Пустой список = показывать все файлы.
    void setExtensions(const QStringList &extensions);

    // Открыть корневую папку: пересканировать и перестроить дерево.
    // Пустой path или несуществующая папка — очистить дерево.
    void openFolder(const QString &path);

    // Пересканировать текущий корень тем же списком расширений.
    void refresh();

    QString rootPath() const { return m_rootPath; }

    // Синхронизация "вкладка -> выделение": если path внутри корня,
    // выделить и проскроллить к нему. Если path вне корня — снять выделение.
    void selectFile(const QString &path);

signals:
    // Пользователь хочет открыть файл (двойной клик или Enter).
    void openFileRequested(const QString &path);

private slots:
    void onDoubleClicked(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);

private:
    // ---- Сканирование ----
    // Рекурсивно сканирует dir, наполняя parentItem.
    // Возвращает true, если были добавлены хоть какие-то элементы
    // (нужно, чтобы не показывать пустые папки).
    bool scanDirectory(const QString &dirPath, QStandardItem *parentItem);

    // Проверка: файл проходит по расширениям?
    bool extensionMatches(const QString &fileName) const;

    // Проверка: скрытая папка/файл (по имени или атрибуту)?
    static bool isHidden(const QString &name, const QString &fullPath);

    // Построить дерево с нуля.
    void rebuild();

    // Найти item по полному пути (обход модели).
    QStandardItem *findItemByPath(const QString &path) const;

    // ---- UI ----
    void buildUi();

    // ---- Данные ----
    QTreeView          *m_view = nullptr;
    QStandardItemModel *m_model = nullptr;

    QString     m_rootPath;
    QStringList m_extensions;   // без точки, в нижнем регистре
    QIcon       m_folderIcon;
    QIcon       m_fileIcon;
};
