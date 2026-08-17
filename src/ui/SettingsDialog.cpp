#include "ui/SettingsDialog.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const Config &initial, QWidget *parent)
    : QDialog(parent), drawColor(initial.drawColor), textBackground(initial.textBackground) {
    setWindowTitle("ShareXL Settings");

    auto *form = new QFormLayout();

    colorButton = new QPushButton();
    colorButton->setFixedWidth(50);
    connect(colorButton, &QPushButton::clicked, this, &SettingsDialog::pickColor);
    form->addRow("Draw color", colorButton);

    thicknessSpin = new QSpinBox();
    thicknessSpin->setRange(1, 30);
    thicknessSpin->setValue(initial.drawThickness);
    form->addRow("Draw thickness", thicknessSpin);

    fontCombo = new QFontComboBox();
    fontCombo->setCurrentFont(QFont(initial.textFontFamily));
    form->addRow("Text font", fontCombo);

    fontSizeSpin = new QSpinBox();
    fontSizeSpin->setRange(6, 128);
    fontSizeSpin->setValue(initial.textFontSize);
    form->addRow("Text size", fontSizeSpin);

    bgColorButton = new QPushButton();
    bgColorButton->setFixedWidth(50);
    connect(bgColorButton, &QPushButton::clicked, this, &SettingsDialog::pickBgColor);
    form->addRow("Text background", bgColorButton);

    auto *saveDirRow = new QHBoxLayout();
    saveDirEdit = new QLineEdit(initial.saveDirectory);
    auto *browseButton = new QPushButton("Browse...");
    connect(browseButton, &QPushButton::clicked, this, &SettingsDialog::pickFolder);
    saveDirRow->addWidget(saveDirEdit);
    saveDirRow->addWidget(browseButton);
    form->addRow("Save folder", saveDirRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    updateSwatches();
}

void SettingsDialog::pickColor() {
    QColor chosen = QColorDialog::getColor(drawColor, this, "Draw color");
    if (chosen.isValid()) {
        drawColor = chosen;
        updateSwatches();
    }
}

void SettingsDialog::pickBgColor() {
    QColor chosen = QColorDialog::getColor(textBackground.alpha() > 0 ? textBackground : Qt::white, this,
                                            "Text background", QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        textBackground = chosen;
        updateSwatches();
    }
}

void SettingsDialog::pickFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Save folder", saveDirEdit->text());
    if (!dir.isEmpty()) saveDirEdit->setText(dir);
}

void SettingsDialog::updateSwatches() {
    colorButton->setStyleSheet(QString("background-color: %1;").arg(drawColor.name(QColor::HexArgb)));
    bgColorButton->setStyleSheet(textBackground.alpha() > 0
                                      ? QString("background-color: %1;").arg(textBackground.name(QColor::HexArgb))
                                      : "border: 1px dashed gray;");
}

Config SettingsDialog::config() const {
    Config c;
    c.drawColor = drawColor;
    c.drawThickness = thicknessSpin->value();
    c.textFontFamily = fontCombo->currentFont().family();
    c.textFontSize = fontSizeSpin->value();
    c.textBackground = textBackground;
    c.saveDirectory = saveDirEdit->text();
    return c;
}
