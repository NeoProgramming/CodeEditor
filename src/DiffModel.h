#pragma once

#include <QString>
#include <QVector>
#include <QHash>

// ---- Типы ----

enum class DiffOp
{
    Equal,     // строки совпадают
    Delete,    // строка есть только в primary
    Insert,    // строка есть только в secondary
    Replace    // пара: primary-строка заменена secondary-строкой
};

// Одна «выровненная» строка. Для Insert primaryText пуст, для Delete —
// secondaryText пуст. Для Replace оба текста непустые (но могут быть
// пустыми строками исходных файлов — это не то же самое, что "нет строки";
// различие определяется op).
struct AlignedLine
{
    DiffOp op = DiffOp::Equal;
    QString primaryText;
    QString secondaryText;
    int primaryFileLine   = -1;  // номер в оригинальном primary (-1 если нет)
    int secondaryFileLine = -1;  // номер в оригинальном secondary (-1 если нет)
    int hunkIndex         = -1;  // индекс hunka (-1 для Equal)
};

// Группа последовательных не-Equal строк.
struct DiffHunk
{
    int firstAlignedLine = 0;   // индекс в alignedLines()
    int lineCount        = 0;   // сколько выровненных строк в hunk'e
    bool hasDeletes = false;    // есть Delete или Replace (что-то уйдёт из primary)
    bool hasInserts = false;    // есть Insert или Replace (что-то придёт в primary)
};

// Пословный diff внутри пары Replace — для подсветки.
struct WordSpan
{
    int start = 0;   // позиция в тексте (в символах)
    int length = 0;
};
struct WordDiff
{
    QVector<WordSpan> primaryChanged;    // изменённые/удалённые фрагменты в primary-строке
    QVector<WordSpan> secondaryChanged;  // изменённые/добавленные фрагменты в secondary-строке
};

// ---- Модель ----

class DiffModel
{
public:
    DiffModel() = default;

    // Пересчитать по двум текстам. ignoreWhitespace — нормализовать
    // trailing whitespace и \r\n перед сравнением (но тексты в AlignedLine
    // остаются оригинальными).
    void compute(const QString &primaryText,
                 const QString &secondaryText,
                 bool ignoreWhitespace = true);

    bool isEmpty() const { return m_aligned.isEmpty(); }

    const QVector<AlignedLine> &alignedLines() const { return m_aligned; }
    const QVector<DiffHunk>    &hunks()        const { return m_hunks; }

    // «Раздутые» тексты для загрузки в редакторы: с пустыми строками-
    // заполнителями, чтобы номера строк совпадали.
    QString primaryAlignedText()   const;
    QString secondaryAlignedText() const;

    // Карта выровненных строк: editor line (0-based) -> file line (0-based)
    // или -1, если это заполнитель. Для primary и secondary отдельно.
    int primaryFileLineForEditorLine(int editorLine) const;
    int secondaryFileLineForEditorLine(int editorLine) const;

    // Обратная карта: file line -> editor line (или -1).
    int primaryEditorLineForFileLine(int fileLine) const;
    int secondaryEditorLineForFileLine(int fileLine) const;

    // Пословный diff для пары Replace (по индексу выровненной строки).
    // Если строка не Replace — пустой результат.
    WordDiff wordDiffFor(int alignedLineIndex) const;

    // Поиск hunka по индексу выровненной строки (для навигации от курсора).
    int hunkIndexForAlignedLine(int alignedLine) const;

    int alignedCount() const { return m_aligned.size(); }
    int hunkCount()    const { return m_hunks.size(); }

private:
    QVector<AlignedLine> m_aligned;
    QVector<DiffHunk>    m_hunks;

    // Кэш пословных diff'ов (индекс выровненной строки -> WordDiff).
    // Ленивый: заполняется по запросу wordDiffFor().
    mutable QHash<int, WordDiff> m_wordCache;
};
