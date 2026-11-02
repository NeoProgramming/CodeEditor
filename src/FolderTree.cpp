#include "FolderTree.h"

#include <QTreeView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QUrl>
#include <QSettings>
#include <QHeaderView>

// ---------------------------------------------------------------- ctor

FolderTree::FolderTree(QWidget *parent)
    : QDockWidget(parent)
{
    setWindowTitle("Explorer");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetMovable
                | QDockWidget::DockWidgetFloatable);

    // Дефолтный набор расширений — C/C++.
    m_extensions = QStringList{ "c", "h", "cpp", "hpp", "cc", "cxx", "hxx" };

    buildUi();

    // Иконки — стандартные из QFileIconProvider.
    QFileIconProvider provider;
    m_folderIcon = provider.icon(QFileIconProvider::Folder);
    m_fileIcon   = provider.icon(QFileIconProvider::File);

    // Восстанавливаем последний корень из QSettings.
    QSettings settings;
    const QString last = settings.value("Explorer/rootPath").toString();
    if (!last.isEmpty() && QFileInfo(last).isDir())
        openFolder(last);
}

void FolderTree::buildUi()
{
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({ "Name" });

    m_view = new QTreeView(this);
    m_view->setModel(m_model);
    m_view->setHeaderHidden(true);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setUniformRowHeights(true);
    m_view->setSortingEnabled(false);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setExpandsOnDoubleClick(true);

    setWidget(m_view);

    connect(m_view, &QTreeView::doubleClicked,
            this, &FolderTree::onDoubleClicked);
    connect(m_view, &QTreeView::customContextMenuRequested,
            this, &FolderTree::onContextMenu);
}

// ---------------------------------------------------------------- public

void FolderTree::setExtensions(const QStringList &extensions)
{
    m_extensions.clear();
    for (const QString &e : extensions) {
        QString s = e.trimmed();
        if (s.startsWith('*')) s.remove(0, 1);
        if (s.startsWith('.')) s.remove(0, 1);
        if (!s.isEmpty())
            m_extensions.append(s.toLower());
    }
    // Если корень уже задан — перестроить с новым фильтром.
    if (!m_rootPath.isEmpty())
        rebuild();
}

void FolderTree::openFolder(const QString &path)
{
    QFileInfo fi(path);
    if (path.isEmpty() || !fi.isDir()) {
        m_rootPath.clear();
        m_model->clear();
        setWindowTitle("Explorer");
        return;
    }

    m_rootPath = fi.absoluteFilePath();

    QSettings settings;
    settings.setValue("Explorer/rootPath", m_rootPath);

    rebuild();
}

void FolderTree::refresh()
{
    if (!m_rootPath.isEmpty())
        rebuild();
}

void FolderTree::selectFile(const QString &path)
{
    if (!m_model || path.isEmpty() || m_rootPath.isEmpty())
        return;

    // Файл вне корня — снять выделение.
    const QString abs = QFileInfo(path).absoluteFilePath();
    if (!abs.startsWith(m_rootPath + QDir::separator())
        && abs != m_rootPath) {
        m_view->clearSelection();
        return;
    }

    QStandardItem *item = findItemByPath(abs);
    if (!item) {
        m_view->clearSelection();
        return;
    }

    const QModelIndex idx = item->index();
    m_view->setCurrentIndex(idx);
    m_view->scrollTo(idx, QAbstractItemView::EnsureVisible);
    // Раскрыть всех родителей, чтобы файл был виден.
    QModelIndex p = idx.parent();
    while (p.isValid()) {
        m_view->expand(p);
        p = p.parent();
    }
}

// ---------------------------------------------------------------- scanning

bool FolderTree::extensionMatches(const QString &fileName) const
{
    if (m_extensions.isEmpty())
        return true;   // пустой список = все файлы

    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix.isEmpty())
        return false;

    return m_extensions.contains(suffix);
}

bool FolderTree::isHidden(const QString &name, const QString &fullPath)
{
    if (name.startsWith('.'))
        return true;
    return QFileInfo(fullPath).isHidden();
}

bool FolderTree::scanDirectory(const QString &dirPath,
                               QStandardItem *parentItem)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return false;

    // Сначала папки, потом файлы — обе группы по алфавиту.
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoSymLinks,
        QDir::NoSort);

    QVector<QFileInfo> dirs;
    QVector<QFileInfo> files;
    for (const QFileInfo &fi : entries) {
        const QString name = fi.fileName();
        if (name == "." || name == "..")
            continue;
        if (isHidden(name, fi.absoluteFilePath()))
            continue;
        if (fi.isSymLink())
            continue;   // симлинки игнорируем полностью
        if (fi.isDir())
            dirs.append(fi);
        else if (fi.isFile() && extensionMatches(name))
            files.append(fi);
    }

    auto byName = [](const QFileInfo &a, const QFileInfo &b) {
        return a.fileName().compare(b.fileName(),
                                    Qt::CaseInsensitive) < 0;
    };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);

    bool anyAdded = false;

    // --- Папки ---
    for (const QFileInfo &fi : dirs) {
        auto *folderItem = new QStandardItem(m_folderIcon, fi.fileName());
        folderItem->setData(fi.absoluteFilePath(), Qt::UserRole);
        folderItem->setEditable(false);

        // Сканируем вглубь. Если ничего не нашли — не добавляем пустую папку.
        const bool hasChildren =
            scanDirectory(fi.absoluteFilePath(), folderItem);
        if (hasChildren) {
            parentItem->appendRow(folderItem);
            anyAdded = true;
        }
        else {
            delete folderItem;
        }
    }

    // --- Файлы ---
    for (const QFileInfo &fi : files) {
        auto *fileItem = new QStandardItem(m_fileIcon, fi.fileName());
        fileItem->setData(fi.absoluteFilePath(), Qt::UserRole);
        fileItem->setEditable(false);
        parentItem->appendRow(fileItem);
        anyAdded = true;
    }

    return anyAdded;
}

void FolderTree::rebuild()
{
    m_model->clear();
    m_model->setHorizontalHeaderLabels({ "Name" });

    if (m_rootPath.isEmpty()) {
        setWindowTitle("Explorer");
        return;
    }

    setWindowTitle("Explorer — " + QFileInfo(m_rootPath).fileName());

    // Корневой item — невидимый контейнер, чтобы дерево начиналось
    // сразу с содержимого корня, а не с самого корня.
    auto *rootItem = m_model->invisibleRootItem();
    scanDirectory(m_rootPath, rootItem);

    m_view->expandAll();   // маленькая утилита — можно раскрыть всё
}

QStandardItem *FolderTree::findItemByPath(const QString &path) const
{
    if (!m_model) return nullptr;

    // Обход в глубину.
    std::function<QStandardItem*(QStandardItem*)> walk =
        [&](QStandardItem *parent) -> QStandardItem* {
        if (!parent) return nullptr;
        for (int r = 0; r < parent->rowCount(); ++r) {
            QStandardItem *child = parent->child(r);
            if (!child) continue;
            if (child->data(Qt::UserRole).toString() == path)
                return child;
            if (QStandardItem *found = walk(child))
                return found;
        }
        return nullptr;
    };
    return walk(m_model->invisibleRootItem());
}

// ---------------------------------------------------------------- UI slots

void FolderTree::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    auto *item = m_model->itemFromIndex(index);
    if (!item) return;

    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;

    if (QFileInfo(path).isFile())
        emit openFileRequested(path);
    // По папке — стандартное поведение QTreeView (expand/collapse).
}

void FolderTree::onContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    auto *item = index.isValid() ? m_model->itemFromIndex(index) : nullptr;
    const QString path = item ? item->data(Qt::UserRole).toString() : QString();
    const bool isFile = !path.isEmpty() && QFileInfo(path).isFile();

    QMenu menu(this);

    QAction *openAct = menu.addAction("Open");
    openAct->setEnabled(isFile);
    connect(openAct, &QAction::triggered, this, [this, path]() {
        emit openFileRequested(path);
    });

    QAction *containingAct = menu.addAction("Open Containing Folder");
    containingAct->setEnabled(!path.isEmpty());
    connect(containingAct, &QAction::triggered, this, [path]() {
        const QFileInfo fi(path);
        const QString dir = fi.isDir() ? fi.absoluteFilePath()
                                       : fi.absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });

    QAction *copyPathAct = menu.addAction("Copy Full Path");
    copyPathAct->setEnabled(!path.isEmpty());
    connect(copyPathAct, &QAction::triggered, this, [path]() {
        QApplication::clipboard()->setText(path);
    });

    menu.addSeparator();

    QAction *refreshAct = menu.addAction("Refresh");
    refreshAct->setEnabled(!m_rootPath.isEmpty());
    connect(refreshAct, &QAction::triggered, this, &FolderTree::refresh);

    menu.exec(m_view->viewport()->mapToGlobal(pos));
}