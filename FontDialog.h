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
class QTreeWidget;
class QTreeWidgetItem;

class FontDialog : public QDialog
{
	Q_OBJECT

public:
	FontDialog(FontManager *fontManager,
		const QVector<SyntaxElementStyle> &elements,
		QWidget *parent = nullptr);

	// –езультаты после нажати€ OK
	int cellWidth()  const;
	int cellHeight() const;
	QVector<SyntaxElementStyle> elements() const;

private slots:
	void onElementSelected(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	
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
	void populateTree();
	void refreshElementSummary(QTreeWidgetItem *item, int elementIndex = -1);
	void loadFromModel();
	void updateColorButton();
	void refreshFontList();
	void refreshPreview();
	
	FontManager *m_fontManager;
	QVector<SyntaxElementStyle> m_elements;

	// ¬ерхн€€ панель
	QSpinBox *m_cellWidthSpin = nullptr;
	QSpinBox *m_cellHeightSpin = nullptr;
	QPushButton *m_calcHeightBtn = nullptr;

	// Ћева€ панель Ч дерево
	QTreeWidget *m_elementTree = nullptr;
	QTreeWidgetItem *m_currentItem = nullptr;

	// ѕрава€ панель Ч редактор элемента
	QGroupBox   *m_editorBox = nullptr;
	QComboBox   *m_fontCombo = nullptr;
	QCheckBox   *m_boldCheck = nullptr;
	QCheckBox   *m_italicCheck = nullptr;
	QPushButton *m_colorButton = nullptr;
	QLabel      *m_metricsLabel = nullptr;

	// Ќижн€€ панель Ч превью
	StylePreview *m_preview = nullptr;
};
