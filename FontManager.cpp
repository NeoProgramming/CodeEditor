// fontmanager.cpp
#include "FontManager.h"
#include <QFontDatabase>
#include <QDebug>

FontManager::FontManager(QObject *parent)
	: QObject(parent)
	, m_cellWidth(12)
	, m_cellHeight(20) 
{
}

void FontManager::setCellSize(int width, int height) 
{
	m_cellWidth = width;
	m_cellHeight = height;

	// Перекалибровать все загруженные шрифты
	for (auto &entry : m_fonts) {
		calibrateFont(entry);
	}
}

int FontManager::addFont(const QString &family, bool bold, bool italic) 
{
	FontEntry entry;
	entry.family = family;
	entry.bold = bold;
	entry.italic = italic;
	entry.pointSize = 10; // Заглушка
	
	if (!calibrateFont(entry)) {
		qWarning() << "Failed to calibrate font:" << family;
		return -1;
	}
	
	m_fonts.append(entry);
	int index = m_fonts.size() - 1;

	// Обновляем метрики
//	m_fonts[index].metrics = getGlyphMetrics(index);

	return index;
}



GlyphMetrics FontManager::getGlyphMetrics(int index) const 
{
	if (index < 0 || index >= m_fonts.size()) {
		return GlyphMetrics{ 0, 0, 0, 0 };
	}
	return m_fonts[index].metrics;

//	QFontMetrics metrics(m_fonts[index].font);
//	GlyphMetrics gm;
//	gm.width = metrics.horizontalAdvance('W');
//	gm.height = metrics.height();
//	gm.ascent = metrics.ascent();
//	gm.descent = metrics.descent();
//	return gm;
}

QFont FontManager::getFont(int index) const 
{
	if (index < 0 || index >= m_fonts.size()) {
		return QFont();
	}
	return m_fonts[index].font;
}

const FontEntry& FontManager::getFontEntry(int index) const 
{
	static FontEntry emptyEntry;
	if (index < 0 || index >= m_fonts.size()) {
		return emptyEntry;
	}
	return m_fonts[index];
}

bool FontManager::calibrateFont(FontEntry &entry) 
{
	// Ищем размер моноширинного шрифта, при котором ширина символа в точности равен требуемому

	// 1. Создаем базовый QFont
	QFont baseFont(entry.family, 10);
	baseFont.setBold(entry.bold);
	baseFont.setItalic(entry.italic);
	baseFont.setStyleHint(QFont::Monospace);

	// 2. Поиск подходящего pointSize
	int pointSize = 10;				// начальное значение в точках
	const int maxAttempts = 200;	// количество попыток

	for (int attempt = 0; attempt < maxAttempts; ++attempt) {
		QFont testFont = baseFont;
		testFont.setPointSize(pointSize);
		QFontMetrics fm(testFont);

		int charWidth = fm.horizontalAdvance('M'); // Эталонный символ
		int charHeight = fm.height();
		int ascent = fm.ascent();
		int descent = fm.descent();

		// 3. Проверка: совпадает ли ширина с целевой
		if (charWidth == m_cellWidth) {
			// Идеальное совпадение
			entry.font = testFont;
			entry.pointSize = pointSize;
			entry.metrics = { charWidth, charHeight, ascent, descent };
			return true;
		}

		// 4. Если ширина меньше целевой — увеличиваем размер
		if (charWidth < m_cellWidth) {
			++pointSize;
		}
		else {
			// Если ширина больше целевой — уменьшаем
			--pointSize;
		}

		// Защита от бесконечного цикла
		if (pointSize < 1 || pointSize > 100) break;
	}

	// 5. Если точного совпадения нет — выбираем ближайшее
	//    Проходим все размеры от 1 до 100 и ищем минимальную разницу
	int bestPointSize = 10;
	int bestDiff = INT_MAX;
	QFont bestFont;

	for (int ps = 1; ps <= 100; ++ps) {
		QFont testFont = baseFont;
		testFont.setPointSize(ps);
		QFontMetrics fm(testFont);

		int width = fm.horizontalAdvance('M');
		int diff = std::abs(width - m_cellWidth);

		if (diff < bestDiff) {
			bestDiff = diff;
			bestPointSize = ps;
			bestFont = testFont;
		}
	}

	// Проверяем, что разница приемлема (например, не более 1 пикселя)
	if (bestDiff <= 1) {
		QFontMetrics fm(bestFont);
		entry.font = bestFont;
		entry.pointSize = bestPointSize;
		entry.metrics = {
			fm.horizontalAdvance('M'),
			fm.height(),
			fm.ascent(),
			fm.descent()
		};
		return true;
	}

	return false;
}
