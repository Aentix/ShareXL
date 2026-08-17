#pragma once

#include <QDialog>

#include "services/Config.h"

class QLineEdit;
class QSpinBox;
class QFontComboBox;
class QPushButton;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const Config &initial, QWidget *parent = nullptr);
    Config config() const;

private:
    QPushButton *colorButton;
    QSpinBox *thicknessSpin;
    QFontComboBox *fontCombo;
    QSpinBox *fontSizeSpin;
    QPushButton *bgColorButton;
    QLineEdit *saveDirEdit;

    QColor drawColor;
    QColor textBackground;

    void pickColor();
    void pickBgColor();
    void pickFolder();
    void updateSwatches();
};
