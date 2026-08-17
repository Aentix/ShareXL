#include "services/Config.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {
QString configPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/sharexl";
    QDir().mkpath(dir);
    return dir + "/sharexl.conf";
}
}

Config Config::load() {
    Config config;
    QSettings settings(configPath(), QSettings::IniFormat);

    config.drawColor = QColor(settings.value("drawColor", config.drawColor.name(QColor::HexArgb)).toString());
    config.drawThickness = settings.value("drawThickness", config.drawThickness).toInt();
    config.textFontFamily = settings.value("textFontFamily", config.textFontFamily).toString();
    config.textFontSize = settings.value("textFontSize", config.textFontSize).toInt();
    config.textBackground =
        QColor(settings.value("textBackground", config.textBackground.name(QColor::HexArgb)).toString());

    const QString defaultSaveDir =
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/Screenshots";
    config.saveDirectory = settings.value("saveDirectory", defaultSaveDir).toString();

    return config;
}

void Config::save() const {
    QSettings settings(configPath(), QSettings::IniFormat);
    settings.setValue("drawColor", drawColor.name(QColor::HexArgb));
    settings.setValue("drawThickness", drawThickness);
    settings.setValue("textFontFamily", textFontFamily);
    settings.setValue("textFontSize", textFontSize);
    settings.setValue("textBackground", textBackground.name(QColor::HexArgb));
    settings.setValue("saveDirectory", saveDirectory);
}
