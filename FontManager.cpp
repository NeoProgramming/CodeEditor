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

	emit cellSizeChanged();
	emit fontsChanged();
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

int FontManager::recommendedLineHeight() const
{
	int maxH = 0;
	for (const FontEntry &entry : m_fonts) {
		if (entry.metrics.height > maxH)
			maxH = entry.metrics.height;
	}
	return maxH;
}

bool FontManager::calibrateFont(FontEntry &entry) 
{
	// Ищем размер моноширинного шрифта, при котором ширина символа в точности равен требуемому

	QFont baseFont(entry.family);
	baseFont.setBold(entry.bold);
	baseFont.setItalic(entry.italic);
	baseFont.setStyleHint(QFont::Monospace);
	baseFont.setFixedPitch(true);

	auto tryFont = [&](const QFont &f) -> bool {
		QFontMetrics fm(f);
		const int w = fm.horizontalAdvance(QLatin1Char('M'));
		const int h = fm.height();
		if (w == m_cellWidth && h <= m_cellHeight) {
			entry.font = f;
			entry.pointSize = f.pointSize(); // может быть -1, если задан pixelSize
			entry.metrics = { w, h, fm.ascent(), fm.descent() };
			return true;
		}
		return false;
	};

	// Перебор по pixelSize — самая плотная сетка
	for (int px = 4; px <= 96; ++px) {
		QFont f = baseFont;
		f.setPixelSize(px);
		if (tryFont(f))
			return true;
	}

	return false;
}

// fontmanager.cpp
void FontManager::clear()
{
	m_fonts.clear();
	emit fontsChanged();
}
