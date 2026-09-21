#pragma once

#include <QDialog>
#include <QVector>

#include "syntaxstyle.h"

class FontManager;
class QSpinBox;
class QListWidget;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QGroupBox;
class StylePreview;
class QVBoxLayout;
class QHBoxLayout;

class FontDialog : public QDialog
{
	Q_OBJECT

public:
	FontDialog(FontManager *fontManager,
		const QVector<SyntaxElementStyle> &elements,
		QWidget *parent = nullptr);

	// Результаты после нажатия OK
	int cellWidth()  const;
	int cellHeight() const;
	QVector<SyntaxElementStyle> elements() const;

private slots:
	void onElementSelected(int row);
	void onFontFamilyChanged(const QString &family);
	void onBoldChanged(bool checked);
	void onItalicChanged(bool checked);
	void onColorClicked();
	void onCalculateHeight();
	void onCellWidthChanged(int w);
	void onCellHeightChanged(int h);
	void onAccepted();

private:
	void buildUi();
	void buildTopPanel(QVBoxLayout *root);
	void buildCentralPanel(QVBoxLayout *root);
	void buildBottomPanel(QVBoxLayout *root);

	void loadFromModel();
	void saveToModel();
	void refreshFontList();
	void refreshPreview();
	void refreshElementListLabels();

	// Проверка: влезает ли текущий стиль в знакоместо
	bool validateElement(const SyntaxElementStyle &s, QString *why = nullptr) const;

	// Собрать QFont из стиля (с калибровкой через FontManager)
	QFont buildCalibratedFont(const SyntaxElementStyle &s) const;

	FontManager *m_fontManager;
	QVector<SyntaxElementStyle> m_elements;

	// Верхняя панель
	QSpinBox *m_cellWidthSpin = nullptr;
	QSpinBox *m_cellHeightSpin = nullptr;
	QPushButton *m_calcHeightBtn = nullptr;

	// Левая панель
	QListWidget *m_elementList = nullptr;

	// Правая панель — редактор элемента
	QGroupBox   *m_editorBox = nullptr;
	QComboBox   *m_fontCombo = nullptr;
	QCheckBox   *m_boldCheck = nullptr;
	QCheckBox   *m_italicCheck = nullptr;
	QPushButton *m_colorButton = nullptr;
	QLabel      *m_metricsLabel = nullptr;

	// Нижняя панель — превью
	StylePreview *m_preview = nullptr;

	int m_currentRow = -1;
};
