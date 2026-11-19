#pragma once

#include <QString>
#include <QByteArray>

class QTextCodec;

namespace TextCodecUtils
{
    // Результат чтения файла: текст + определённая кодировка.
    struct ReadResult
    {
        QString     text;
        QByteArray  codecName;   // имя кодека, которым был прочитан файл
        bool        ok = false;
    };

    // Прочитать файл, автоматически определив кодировку.
    // Алгоритм:
    //   1. BOM (UTF-8/16/32) — если есть, используем;
    //   2. иначе — валидный ли UTF-8 без BOM; если да — UTF-8;
    //   3. иначе — codecForLocale() (CP1251 на русской Windows и т.п.).
    // В случае ошибки открытия/чтения — ok = false, error заполняется.
    ReadResult readFile(const QString &path, QString *error = nullptr);

    // Записать текст в файл с указанной кодировкой.
    // codecName пустой — используем UTF-8.
    bool writeFile(const QString &path,
                   const QString &text,
                   const QByteArray &codecName,
                   QString *error = nullptr);

    // Получить QTextCodec* по имени. Если имя пустое или неизвестное —
    // возвращает UTF-8.
    QTextCodec *codecForNameOrDefault(const QByteArray &name);

    // Список доступных кодеков для UI (отсортирован по имени).
    // Включает только "читаемые" кодеки (без псевдо-кодеков вроде System).
    QList<QByteArray> availableCodecNames();

} // namespace TextCodecUtils
