#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>

// Простейшее локальное версионирование: версии - это файлы вида
// <file>.~YYYYMMDD_HHMMSS, лежащие рядом с рабочим файлом.
// Рабочий файл <file> - единственный изменяемый; версии read-only по смыслу.
// Никакого состояния в памяти: все операции - над файлами на диске.
namespace VersionStore
{
    // Список версий filePath (полные пути), отсортирован по возрастанию
    // времени (самая старая - первая). Пустой список, если версий нет.
    QStringList list(const QString &filePath);

    // Создать версию: filePath -> filePath.~<mtime>.
    // Если такой таймштамп уже занят, mtime filePath искусственно
    // увеличивается на секунду в будущее до свободного.
    // Возвращает путь созданной версии или пустую строку при ошибке.
    QString create(const QString &filePath, QString *error = nullptr);

    // Сделать versionPath рабочим файлом:
    //   1. filePath -> filePath.~<mtime(filePath)>   (переименование)
    //   2. versionPath -> filePath                    (переименование)
    // Версия versionPath на диске исчезает (становится filePath).
    bool accept(const QString &filePath, const QString &versionPath,
                QString *error = nullptr);

    // Записать text в filePath (для "Accept buffer").
    bool writeCurrent(const QString &filePath, const QString &text,
                      QString *error = nullptr);

    // Удалить одну версию.
    bool remove(const QString &versionPath, QString *error = nullptr);

    // Удалить все версии filePath. Возвращает количество удалённых.
    int removeAll(const QString &filePath, QString *error = nullptr);

    // Удалить все версии всех файлов под rootPath (рекурсивно,
    // с пропуском скрытых папок и симлинков). Возвращает количество.
    int removeAllUnder(const QString &rootPath, QString *error = nullptr);

    // Является ли path версией (имя содержит ".~" + 8+ цифр).
    bool isVersionPath(const QString &path);

} // namespace VersionStore
