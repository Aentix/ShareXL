#include "services/Platform.h"

#include <QGuiApplication>

namespace Platform {

bool isWayland() { return QGuiApplication::platformName() == QLatin1String("wayland"); }

bool isX11() { return QGuiApplication::platformName() == QLatin1String("xcb"); }

bool isKdePlasma() { return QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP")).contains("KDE"); }

}
