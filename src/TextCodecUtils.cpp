#include "TextCodecUtils.h"

#include <QFile>
#include <QTextCodec>
#include <QDebug>

namespace TextCodecUtils
{

// ---------------------------------------------------------------- detect

// Проверка: являются ли байты валидным UTF-8 (без BOM).
// Используем toUnicode с state: если state != 0 — были ошибки.
static bool isValidUtf8(const QByteArray &bytes)
{
    QTextCodec *utf8 = QTextCodec::codecForName("UTF-8");
    if (!utf8)
        return false;

	QTextCodec::ConverterState  state = 0;
    utf8->toUnicode(bytes.constData(), bytes.size(), &state);
    return state.invalidChars == 0;
}

ReadResult readFile(const QString &path, QString *error)
{
    ReadResult result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return result;
    }

    const QByteArray raw = file.readAll();
    file.close();

    // 1. BOM — QTextCodec::codecForUtfText распознаёт UTF-8/16/32 BOM.
    //    Если BOM нет — возвращает nullptr (второй аргумент — default).
    if (QTextCodec *bomCodec = QTextCodec::codecForUtfText(raw, nullptr)) {
        result.text = bomCodec->toUnicode(raw);
        result.codecName = bomCodec->name();
        result.ok = true;
        return result;
    }

    // 2. UTF-8 без BOM — проверяем валидность.
    if (isValidUtf8(raw)) {
        QTextCodec *utf8 = QTextCodec::codecForName("UTF-8");
        result.text = utf8->toUnicode(raw);
        result.codecName = "UTF-8";
        result.ok = true;
        return result;
    }

    // 3. Fallback — системная кодировка.
    QTextCodec *locale = QTextCodec::codecForLocale();
    result.text = locale->toUnicode(raw);
    result.codecName = locale->name();
    result.ok = true;
    return result;
}

// ---------------------------------------------------------------- write

QTextCodec *codecForNameOrDefault(const QByteArray &name)
{
    if (name.isEmpty())
        return QTextCodec::codecForName("UTF-8");

    if (QTextCodec *c = QTextCodec::codecForName(name))
        return c;

    return QTextCodec::codecForName("UTF-8");
}

bool writeFile(const QString &path,
               const QString &text,
               const QByteArray &codecName,
               QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = file.errorString();
        return false;
    }

    QTextCodec *codec = codecForNameOrDefault(codecName);
    const QByteArray encoded = codec->fromUnicode(text);

    if (file.write(encoded) != encoded.size()) {
        if (error) *error = file.errorString();
        return false;
    }

    file.close();
    return true;
}

// ---------------------------------------------------------------- list

QList<QByteArray> availableCodecNames()
{
    QList<QByteArray> names = QTextCodec::availableCodecs();

    // Убираем псевдо-кодеки и дубликаты.
    QList<QByteArray> result;
    result.reserve(names.size());
    QSet<QByteArray> seen;
    for (const QByteArray &n : names) {
        // "System" — псевдо-кодек, пропускаем.
        if (n.compare("System", Qt::CaseInsensitive) == 0)
            continue;
        const QByteArray lower = n.toLower();
        if (seen.contains(lower))
            continue;
        seen.insert(lower);
        result.append(n);
    }

    std::sort(result.begin(), result.end(),
              [](const QByteArray &a, const QByteArray &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return result;
}

} // namespace TextCodecUtils
