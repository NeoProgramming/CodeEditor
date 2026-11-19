#include "VersionStore.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QTextCodec>


// Формат таймштампа в имени версии: YYYYMMDD_HHMMSS.
QString stampToString(const QDateTime &dt)
{
    return dt.toString("yyyyMMdd_HHmmss");
}

QString versionPathFor(const QString &filePath, const QDateTime &when)
{
    return filePath + ".~" + stampToString(when);
}

// Регулярка: путь заканчивается на ".~" + 8 цифр + "_" + 6 цифр.
const QRegularExpression &versionSuffixRe()
{
    static const QRegularExpression re(
        QStringLiteral("\\.~\\d{8}_\\d{6}$"));
    return re;
}

bool pathIsHidden(const QString &name, const QString &fullPath)
{
    if (name.startsWith('.'))
        return true;
    return QFileInfo(fullPath).isHidden();
}


namespace VersionStore
{

QStringList list(const QString &filePath)
{
    QStringList result;
    if (filePath.isEmpty())
        return result;

    const QFileInfo fi(filePath);
    const QDir dir = fi.absoluteDir();
    const QString baseName = fi.fileName();
    const QString prefix = baseName + ".~";

    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::NoSymLinks, QDir::NoSort);

    for (const QFileInfo &e : entries) {
        const QString name = e.fileName();
        if (!name.startsWith(prefix))
            continue;
        if (!versionSuffixRe().match(name).hasMatch())
            continue;
        result.append(e.absoluteFilePath());
    }

    // Сортировка по имени = по таймштампу = по времени.
    std::sort(result.begin(), result.end());
    return result;
}

QString create(const QString &filePath, QString *error)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        if (error) *error = QStringLiteral("File does not exist: %1").arg(filePath);
        return QString();
    }

    const QFileInfo fi(filePath);
    QDateTime when = fi.lastModified();

    // Разрешаем коллизию: пока такой таймштамп занят, +1 сек.
    QString target = versionPathFor(filePath, when);
    while (QFileInfo::exists(target)) {
        when = when.addSecs(1);
        target = versionPathFor(filePath, when);
    }

    if (!QFile::copy(filePath, target)) {
        if (error) *error = QStringLiteral("Cannot create version: %1").arg(target);
        return QString();
    }

    // Выставляем mtime копии равным вычисленному времени,
    // чтобы имя и реальное время совпадали.
    QFile f(target);
    if (f.open(QIODevice::ReadWrite)) {
        f.setFileTime(when, QFileDevice::FileModificationTime);
        f.close();
    }

    return target;
}

bool accept(const QString &filePath, const QString &versionPath,
            QString *error)
{
    if (!QFileInfo::exists(filePath)) {
        if (error) *error = QStringLiteral("Working file does not exist: %1").arg(filePath);
        return false;
    }
    if (!QFileInfo::exists(versionPath)) {
        if (error) *error = QStringLiteral("Version file does not exist: %1").arg(versionPath);
        return false;
    }

    // 1. Текущий рабочий файл -> новая версия с его mtime.
    //    Если таймштамп занят (бывает при "принять только что созданную версию"),
    //    увеличиваем на секунду.
    const QFileInfo fi(filePath);
    QDateTime when = fi.lastModified();
    QString snapshot = versionPathFor(filePath, when);
    while (QFileInfo::exists(snapshot)) {
        when = when.addSecs(1);
        snapshot = versionPathFor(filePath, when);
    }

    if (!QFile::rename(filePath, snapshot)) {
        if (error) *error = QStringLiteral("Cannot rename working file to version: %1").arg(snapshot);
        return false;
    }

    // 2. Выбранная версия -> рабочий файл.
    if (!QFile::rename(versionPath, filePath)) {
        // Откат: вернуть рабочий файл на место.
        QFile::rename(snapshot, filePath);
        if (error) *error = QStringLiteral("Cannot rename version to working file: %1").arg(versionPath);
        return false;
    }

    return true;
}

bool writeCurrent(const QString &filePath, const QString &text,
	const QByteArray &codecName, QString *error)
{
	QFile f(filePath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		if (error) *error = f.errorString();
		return false;
	}

	QTextCodec *codec = codecName.isEmpty()
		? QTextCodec::codecForName("UTF-8")
		: QTextCodec::codecForName(codecName);
	if (!codec)
		codec = QTextCodec::codecForName("UTF-8");

	const QByteArray bytes = codec->fromUnicode(text);
	if (f.write(bytes) != bytes.size()) {
		if (error) *error = f.errorString();
		return false;
	}
	f.close();
	return true;
}

bool remove(const QString &versionPath, QString *error)
{
    if (!QFileInfo::exists(versionPath)) {
        if (error) *error = QStringLiteral("Version file does not exist: %1").arg(versionPath);
        return false;
    }
    QFile f(versionPath);
    if (!f.remove()) {
        if (error) *error = QStringLiteral("Cannot delete: %1").arg(versionPath);
        return false;
    }
    return true;
}

int removeAll(const QString &filePath, QString *error)
{
    const QStringList versions = list(filePath);
    int removed = 0;
    for (const QString &v : versions) {
        QString err;
        if (remove(v, &err))
            ++removed;
        else if (error && error->isEmpty())
            *error = err;
    }
    return removed;
}

namespace {

int removeAllUnderImpl(const QString &dirPath, QString *error, int &removed)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return 0;

    const QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoSymLinks, QDir::NoSort);

    for (const QFileInfo &fi : entries) {
        const QString name = fi.fileName();
        if (name == "." || name == "..")
            continue;
        if (pathIsHidden(name, fi.absoluteFilePath()))
            continue;
        if (fi.isSymLink())
            continue;

        if (fi.isDir()) {
            removeAllUnderImpl(fi.absoluteFilePath(), error, removed);
        }
        else if (fi.isFile()) {
            if (!versionSuffixRe().match(name).hasMatch())
                continue;
            QFile f(fi.absoluteFilePath());
            if (f.remove())
                ++removed;
            else if (error && error->isEmpty())
                *error = QStringLiteral("Cannot delete: %1").arg(fi.absoluteFilePath());
        }
    }
    return removed;
}

} // namespace

int removeAllUnder(const QString &rootPath, QString *error)
{
    int removed = 0;
    removeAllUnderImpl(rootPath, error, removed);
    return removed;
}

bool isVersionPath(const QString &path)
{
    return versionSuffixRe().match(path).hasMatch();
}

} // namespace VersionStore
