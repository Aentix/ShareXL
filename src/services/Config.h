#pragma once

#include <QColor>
#include <QString>

struct Config {
    QColor drawColor{237, 28, 36};
    int drawThickness = 3;
    QString textFontFamily{"Sans"};
    int textFontSize = 18;
    QColor textBackground{Qt::transparent};
    QString saveDirectory;

    static Config load();
    void save() const;
};
