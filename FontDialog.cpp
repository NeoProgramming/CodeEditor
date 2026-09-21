#include "FontDialog.h"
#include "FontManager.h"
#include "StylePreview.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QMessageBox>

FontDialog::FontDialog(FontManager *fontManager,
	const QVector<SyntaxElementStyle> &elements,
	QWidget *parent)
	: QDialog(parent)
	, m_fontManager(fontManager)
	, m_elements(elements)
{
	setWindowTitle("Fonts and Syntax Highlighting");
	resize(900, 640);

	buildUi();
	loadFromModel();
}

// ---------------------------------------------------------------- UI

void FontDialog::buildUi()
{
	auto *root = new QVBoxLayout(this);

	buildTopPanel(root);
	buildCentralPanel(root);
	buildBottomPanel(root);

	auto *buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &FontDialog::onAccepted);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	root->addWidget(buttons);
}

void FontDialog::buildTopPanel(QVBoxLayout *root)
{
	auto *box = new QGroupBox("Cell size (pixels)", this);
	auto *lay = new QHBoxLayout(box);

	lay->addWidget(new QLabel("Width:", box));
	m_cellWidthSpin = new QSpinBox(box);
	m_cellWidthSpin->setRange(4, 64);
	m_cellWidthSpin->setValue(m_fontManager->getCellWidth());
	lay->addWidget(m_cellWidthSpin);

	lay->addSpacing(16);
	lay->addWidget(new QLabel("Height:", box));
	m_cellHeightSpin = new QSpinBox(box);
	m_cellHeightSpin->setRange(6, 96);
	m_cellHeightSpin->setValue(m_fontManager->getCellHeight());
	lay->addWidget(m_cellHeightSpin);

	m_calcHeightBtn = new QPushButton("Calculate", box);
	m_calcHeightBtn->setToolTip(
		"Set height to the maximum needed by the used fonts");
	lay->addWidget(m_calcHeightBtn);

	lay->addStretch();
	root->addWidget(box);

	connect(m_cellWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &FontDialog::onCellWidthChanged);
	connect(m_cellHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &FontDialog::onCellHeightChanged);
	connect(m_calcHeightBtn, &QPushButton::clicked,
		this, &FontDialog::onCalculateHeight);
}

void FontDialog::buildCentralPanel(QVBoxLayout * /*unused*/)
{
	// Найдём корневой layout и добавим в него горизонтальный
	auto *root = qobject_cast<QVBoxLayout *>(layout());
	auto *central = new QHBoxLayout();
	root->addLayout(central, 1);

	// ----- Слева: список элементов -----
	auto *leftBox = new QGroupBox("Syntax elements", this);
	auto *leftLay = new QVBoxLayout(leftBox);
	m_elementList = new QListWidget(leftBox);
	leftLay->addWidget(m_elementList);

	// ----- Справа: редактор выбранного элемента -----
	m_editorBox = new QGroupBox("Style", this);
	auto *form = new QFormLayout(m_editorBox);

	m_fontCombo = new QComboBox(m_editorBox);
	form->addRow("Font:", m_fontCombo);

	m_boldCheck = new QCheckBox("Bold", m_editorBox);
	m_italicCheck = new QCheckBox("Italic", m_editorBox);
	auto *styleLay = new QHBoxLayout();
	styleLay->addWidget(m_boldCheck);
	styleLay->addWidget(m_italicCheck);
	styleLay->addStretch();
	form->addRow("Style:", styleLay);

	m_colorButton = new QPushButton("Choose color...", m_editorBox);
	form->addRow("Color:", m_colorButton);

	m_metricsLabel = new QLabel(m_editorBox);
	m_metricsLabel->setWordWrap(true);
	form->addRow("Metrics:", m_metricsLabel);

	central->addWidget(leftBox, 1);
	central->addWidget(m_editorBox, 1);

	connect(m_elementList, &QListWidget::currentRowChanged,
		this, &FontDialog::onElementSelected);
	connect(m_fontCombo, &QComboBox::currentTextChanged,
		this, &FontDialog::onFontFamilyChanged);
	connect(m_boldCheck, &QCheckBox::toggled, this, &FontDialog::onBoldChanged);
	connect(m_italicCheck, &QCheckBox::toggled, this, &FontDialog::onItalicChanged);
	connect(m_colorButton, &QPushButton::clicked, this, &FontDialog::onColorClicked);
}

void FontDialog::buildBottomPanel(QVBoxLayout *root)
{
	auto *box = new QGroupBox("Preview", this);
	auto *lay = new QVBoxLayout(box);

	m_preview = new StylePreview(box);
	m_preview->setMinimumHeight(120);
	lay->addWidget(m_preview);

	root->addWidget(box);
}

// ---------------------------------------------------------------- Model <-> UI

void FontDialog::loadFromModel()
{
	refreshFontList();
	refreshElementListLabels();
	if (!m_elements.isEmpty())
		m_elementList->setCurrentRow(0);
	refreshPreview();
}

void FontDialog::refreshFontList()
{
	// Только моноширинные шрифты
	m_fontCombo->clear();
	const auto families = QFontDatabase().families(QFontDatabase::Any);
	for (const QString &fam : families) {
		if (QFontDatabase().isFixedPitch(fam))
			m_fontCombo->addItem(fam);
	}
	// Добавим текущий, если его вдруг нет в списке (бывает при кастомных шрифтах)
	if (m_currentRow >= 0) {
		const QString cur = m_elements[m_currentRow].fontFamily;
		if (m_fontCombo->findText(cur) < 0 && !cur.isEmpty())
			m_fontCombo->addItem(cur);
	}
}

void FontDialog::refreshElementListLabels()
{
	m_elementList->clear();
	for (const auto &e : m_elements)
		m_elementList->addItem(e.title);
}

void FontDialog::refreshPreview()
{
	m_preview->setData(m_elements,
		m_cellWidthSpin->value(),
		m_cellHeightSpin->value(),
		m_fontManager);
}

// ---------------------------------------------------------------- Slots

void FontDialog::onElementSelected(int row)
{
	if (row < 0 || row >= m_elements.size()) {
		m_currentRow = -1;
		m_editorBox->setEnabled(false);
		return;
	}

	m_currentRow = row;
	m_editorBox->setEnabled(true);

	const SyntaxElementStyle &e = m_elements[row];

	// Обновляем список шрифтов так, чтобы он содержал текущий
	refreshFontList();

	m_fontCombo->blockSignals(true);
	int idx = m_fontCombo->findText(e.fontFamily);
	if (idx < 0 && !e.fontFamily.isEmpty()) {
		m_fontCombo->addItem(e.fontFamily);
		idx = m_fontCombo->findText(e.fontFamily);
	}
	m_fontCombo->setCurrentIndex(idx);
	m_fontCombo->blockSignals(false);

	m_boldCheck->blockSignals(true);
	m_boldCheck->setChecked(e.bold);
	m_boldCheck->blockSignals(false);

	m_italicCheck->blockSignals(true);
	m_italicCheck->setChecked(e.italic);
	m_italicCheck->blockSignals(false);

	refreshPreview();
}

void FontDialog::onFontFamilyChanged(const QString &family)
{
	if (m_currentRow < 0) return;
	m_elements[m_currentRow].fontFamily = family;
	refreshPreview();
}

void FontDialog::onBoldChanged(bool checked)
{
	if (m_currentRow < 0) return;
	m_elements[m_currentRow].bold = checked;
	refreshPreview();
}

void FontDialog::onItalicChanged(bool checked)
{
	if (m_currentRow < 0) return;
	m_elements[m_currentRow].italic = checked;
	refreshPreview();
}

void FontDialog::onColorClicked()
{
	if (m_currentRow < 0) return;
	const QColor c = QColorDialog::getColor(
		m_elements[m_currentRow].color, this, "Text color");
	if (!c.isValid()) return;
	m_elements[m_currentRow].color = c;
	refreshPreview();
}

void FontDialog::onCellWidthChanged(int w)
{
	Q_UNUSED(w);
	refreshPreview();
}

void FontDialog::onCellHeightChanged(int h)
{
	Q_UNUSED(h);
	refreshPreview();
}

void FontDialog::onCalculateHeight()
{
	// Временный FontManager с тем же cellWidth и большим «потолком» высоты,
	// чтобы откалибровать все шрифты и получить реальную максимальную высоту.
	FontManager tmp;
	tmp.setCellSize(m_cellWidthSpin->value(), 1000);

	for (const auto &e : m_elements) {
		tmp.addFont(e.fontFamily, e.bold, e.italic);
	}

	const int h = tmp.recommendedLineHeight();
	if (h > 0)
		m_cellHeightSpin->setValue(h);
}

void FontDialog::onAccepted()
{
	// 1. Проверим, что все элементы калибруются при текущем размере ячейки
	FontManager probe;
	probe.setCellSize(m_cellWidthSpin->value(), m_cellHeightSpin->value());

	QStringList failed;
	for (const auto &e : m_elements) {
		if (probe.addFont(e.fontFamily, e.bold, e.italic) < 0)
			failed << e.title;
	}

	if (!failed.isEmpty()) {
		QMessageBox::warning(this, "Cannot apply",
			"The following styles cannot be calibrated to "
			"the chosen cell width:\n\n" + failed.join("\n"));
		return;
	}

	accept();
}

// ---------------------------------------------------------------- Accessors

int FontDialog::cellWidth() const { return m_cellWidthSpin->value(); }
int FontDialog::cellHeight() const { return m_cellHeightSpin->value(); }
QVector<SyntaxElementStyle> FontDialog::elements() const { return m_elements; }
