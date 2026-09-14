#pragma once
#include <QFont>
#include <QFontMetrics>
#include <QVector>
#include <QString>

struct GlyphMetrics {
	int width;
	int height;
	int ascent;
	int descent;
};

struct FontEntry {
	QString family;
	bool bold;
	bool italic;
	int pointSize;  // Реальный размер в пунктах
	QFont font;
	GlyphMetrics metrics;
};

class FontManager : public QObject {
	Q_OBJECT
public:
	explicit FontManager(QObject *parent = nullptr);

	// Установка размера знакоместа
	void setCellSize(int width, int height);

	// Добавление шрифта - возвращает индекс
	int addFont(const QString &family, bool bold = false, bool italic = false);

	// Получение шрифта по индексу
	QFont getFont(int index) const;
	const FontEntry& getFontEntry(int index) const;

	// Получение метрик по индексу
	GlyphMetrics getGlyphMetrics(int index) const;

	// Получение размера знакоместа
	int getCellWidth() const { return m_cellWidth; }
	int getCellHeight() const { return m_cellHeight; }

	// Количество зарегистрированных шрифтов
	int fontCount() const { return m_fonts.size(); }

private:
	QVector<FontEntry> m_fonts;
	int m_cellWidth;
	int m_cellHeight;

	// Основной метод подбора размера
	bool calibrateFont(FontEntry &entry);
};
