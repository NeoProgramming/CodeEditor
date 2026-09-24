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
#include <QTreeWidget>
#include <QHeaderView>
#include <QPainter>
#include <QTimer>
#include <QTreeWidgetItem>

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
	QTimer::singleShot(1, this, &FontDialog::updateColorButton);
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
	m_elementTree = new QTreeWidget(leftBox);
	m_elementTree->setColumnCount(2);
	m_elementTree->setHeaderLabels({ "Element", "Style" });
	m_elementTree->setRootIsDecorated(true);
	m_elementTree->setUniformRowHeights(true);
	m_elementTree->setAlternatingRowColors(true);
	m_elementTree->header()->setStretchLastSection(true);
	m_elementTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_elementTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	leftLay->addWidget(m_elementTree);

	// ----- Справа: редактор выбранного элемента -----
	m_editorBox = new QGroupBox("Style", this);
	auto *form = new QFormLayout(m_editorBox);

	m_fontCombo = new QComboBox(m_editorBox);
	form->addRow("Font:", m_fontCombo);

	m_boldCheck = new QCheckBox("Bold", m_editorBox);
	m_italicCheck = new QCheckBox("Italic", m_editorBox);
	m_colorButton = new QPushButton(m_editorBox);
	m_colorButton->setFixedHeight(24);
	m_colorButton->setMinimumWidth(80);
	m_colorButton->setFlat(false);
	m_colorButton->setAutoFillBackground(true);
	m_colorButton->setText(QString()); // без надписи
	m_colorButton->setToolTip("Choose text color");
	
	auto *styleLay = new QHBoxLayout();
	styleLay->setContentsMargins(0, 0, 0, 0);
	styleLay->addWidget(m_boldCheck);
	styleLay->addWidget(m_italicCheck);
	// Растяжка по центру — всё свободное место уходит сюда
	styleLay->addStretch();
	styleLay->addWidget(new QLabel("Color:", m_editorBox));
	styleLay->addWidget(m_colorButton);
	form->addRow("Style:", styleLay);


	m_metricsLabel = new QLabel(m_editorBox);
	m_metricsLabel->setWordWrap(true);
	form->addRow("Metrics:", m_metricsLabel);

	central->addWidget(leftBox, 1);
	central->addWidget(m_editorBox, 1);

	connect(m_elementTree, &QTreeWidget::currentItemChanged,
		this, &FontDialog::onElementSelected);
	connect(m_fontCombo, &QComboBox::currentTextChanged,
		this, &FontDialog::onFontFamilyChanged);
	connect(m_boldCheck, &QCheckBox::toggled, this, &FontDialog::onBoldChanged);
	connect(m_italicCheck, &QCheckBox::toggled, this, &FontDialog::onItalicChanged);
	connect(m_colorButton, &QPushButton::clicked, this, &FontDialog::onColorClicked);
}

void FontDialog::populateTree()
{
	m_elementTree->clear();

	for (int i = 0; i < m_elements.size(); ++i) {
		const SyntaxElementStyle &e = m_elements[i];

		auto *item = new QTreeWidgetItem(m_elementTree);
		item->setText(0, e.title);
		item->setData(0, Qt::UserRole, i); // индекс элемента

		// Вторая колонка заполняется отдельно
		refreshElementSummary(item, i);
	}
}

void FontDialog::refreshElementSummary(QTreeWidgetItem *item, int elementIndex)
{
	if (!item)
		return;
	if(elementIndex < 0)
		elementIndex = item->data(0, Qt::UserRole).toInt();
	if (elementIndex < 0 || elementIndex >= m_elements.size())
		return;

	const SyntaxElementStyle &e = m_elements[elementIndex];

	// --- Атрибуты ---
	QStringList attrs;
	if (e.bold)   attrs << "bold";
	if (e.italic) attrs << "italic";
	if (attrs.isEmpty()) attrs << "regular";
	const QString attrStr = attrs.join(", ");

	// --- Размер шрифта ---
	// Реальный размер берём из FontManager (после калибровки),
	// а не из m_elements — там его нет.
	QString sizeStr;
	if (m_fontManager && elementIndex < m_fontManager->fontCount()) {
		const FontEntry &fe = m_fontManager->getFontEntry(elementIndex);
	//	if (fe.pointSize > 0)
	//		sizeStr = QString("%1 pt").arg(fe.pointSize);
	//	else 
		if (fe.font.pixelSize() > 0)
			sizeStr = QString("%1 px").arg(fe.font.pixelSize());
	}

	// --- Цвет ---
	// Имя цвета, если оно есть; иначе hex-значение
	QString colorStr = e.color.name(QColor::HexRgb);
	const QString colorName = e.color.name();
	if (colorName != colorStr) // Qt вернул человекочитаемое имя
		colorStr = QString("%1 (%2)").arg(colorName, colorStr);

	// --- Собираем строку ---
	// Пример: Consolas, bold, 11 pt, #0000cc
	QString summary = e.fontFamily;
	if (!attrStr.isEmpty())
		summary += ", " + attrStr;
	if (!sizeStr.isEmpty())
		summary += ", " + sizeStr;
	if (!colorStr.isEmpty())
		summary += ", " + colorStr;

	item->setText(1, summary);

	// Подкрашиваем текст цветом самого стиля — это удобно визуально.
	// Но цвет текста не должен мешать читаемости — если он слишком светлый
	// на светлом фоне, Qt сам не поправит. Поэтому оставляем чёрный,
	// а цвет показываем квадратиком в третьей колонке.

	// Опционально: квадратик цвета в начале второй колонки
	QPixmap swatch(12, 12);
	swatch.fill(e.color);
	item->setIcon(1, QIcon(swatch));
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

void FontDialog::updateColorButton()
{
	if (!m_colorButton)
		return;

	const int idx = m_currentItem ? m_currentItem->data(0, Qt::UserRole).toInt() : -1;
	const QColor c = (idx >= 0)
		? m_elements[idx].color
		: palette().color(QPalette::Button);

	// Рисуем квадратик с рамкой, чтобы светлые цвета были видны
	const int    m_inset = 4;
	const QRect r = m_colorButton->rect().adjusted(m_inset, m_inset, -m_inset, -m_inset);
	const QSize s = QSize(60, r.height());
	QPixmap pm(s);
	pm.fill(c);
	
	m_colorButton->setIcon(QIcon(pm));
	m_colorButton->setIconSize(s);
	m_colorButton->setText(QString());
	m_colorButton->setToolTip(
		QString("Text color: %1").arg(c.name(QColor::HexRgb)));

	// Сбрасываем возможный stylesheet от предыдущего варианта
	m_colorButton->setStyleSheet(QString());
}

// ---------------------------------------------------------------- Model <-> UI

void FontDialog::loadFromModel()
{
	refreshFontList();
	populateTree();
	updateColorButton();
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
}

void FontDialog::refreshPreview()
{
	m_preview->setData(m_elements,
		m_cellWidthSpin->value(),
		m_cellHeightSpin->value(),
		m_fontManager);
}

// ---------------------------------------------------------------- Slots

void FontDialog::onElementSelected(QTreeWidgetItem *current,
	QTreeWidgetItem *previous)
{
	Q_UNUSED(previous);

	m_currentItem = current;
	const int idx = m_currentItem->data(0, Qt::UserRole).toInt();

	m_editorBox->setEnabled(true);

	const SyntaxElementStyle &e = m_elements[idx];

	// --- Синхронизируем правую панель без эмиссии сигналов ---	
	m_fontCombo->blockSignals(true);
	int comboIdx = m_fontCombo->findText(e.fontFamily);
	if (comboIdx < 0 && !e.fontFamily.isEmpty()) {
		m_fontCombo->addItem(e.fontFamily);
		comboIdx = m_fontCombo->findText(e.fontFamily);
	}
	m_fontCombo->setCurrentIndex(comboIdx);
	m_fontCombo->blockSignals(false);

	m_boldCheck->blockSignals(true);
	m_boldCheck->setChecked(e.bold);
	m_boldCheck->blockSignals(false);

	m_italicCheck->blockSignals(true);
	m_italicCheck->setChecked(e.italic);
	m_italicCheck->blockSignals(false);

	updateColorButton();
	refreshPreview();	
}

void FontDialog::onFontFamilyChanged(const QString &family)
{
	if (!m_currentItem) return;
	const int idx = m_currentItem->data(0, Qt::UserRole).toInt();

	m_elements[idx].fontFamily = family;
	
	refreshElementSummary(m_currentItem, idx);
	refreshPreview();
}

void FontDialog::onBoldChanged(bool checked)
{
	if (!m_currentItem) return;
	const int idx = m_currentItem->data(0, Qt::UserRole).toInt();

	SyntaxElementStyle &e = m_elements[idx];
	e.bold = checked;
	m_fontManager->setFont(idx, e.fontFamily, e.bold, e.italic);
	
	refreshElementSummary(m_currentItem, idx);
	refreshPreview();
}

void FontDialog::onItalicChanged(bool checked)
{
	if (!m_currentItem) return;
	const int idx = m_currentItem->data(0, Qt::UserRole).toInt();

	m_elements[idx].italic = checked;
	
	refreshElementSummary(m_currentItem, idx);
	refreshPreview();
}

void FontDialog::onColorClicked()
{
	if (!m_currentItem) return;
	const int idx = m_currentItem->data(0, Qt::UserRole).toInt();

	const QColor c = QColorDialog::getColor(
		m_elements[idx].color, this, "Text color");
	if (!c.isValid()) return;
	m_elements[idx].color = c;

	updateColorButton();
	
	refreshElementSummary(m_currentItem, idx);
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
