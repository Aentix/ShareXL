#include "capture/CaptureBackend.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QUrl>

#include "services/Platform.h"

CaptureBackend::CaptureBackend(QObject *parent) : QObject(parent) {}

void CaptureBackend::requestScreenshot() {
    if (Platform::isX11()) {
        captureX11();
        return;
    }

    QDBusInterface portal(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Screenshot",
        QDBusConnection::sessionBus());

    if (!portal.isValid()) {
        emit failed("xdg-desktop-portal is not available");
        return;
    }

    QVariantMap options;
    options["interactive"] = false;

    QDBusReply<QDBusObjectPath> reply = portal.call("Screenshot", QString(), options);
    if (!reply.isValid()) {
        emit failed(reply.error().message());
        return;
    }

    const QString requestPath = reply.value().path();
    QDBusConnection::sessionBus().connect(
        QString(), requestPath,
        "org.freedesktop.portal.Request", "Response",
        this, SLOT(onResponse(uint, QVariantMap)));
}

void CaptureBackend::onResponse(uint code, const QVariantMap &results) {
    if (code != 0) {
        emit failed(QString("screenshot request was denied or cancelled (code %1)").arg(code));
        return;
    }

    const QString uri = results.value("uri").toString();
    const QString localPath = QUrl(uri).toLocalFile();

    QImage image(localPath);
    if (image.isNull()) {
        emit failed(QString("failed to load screenshot from %1").arg(localPath));
        return;
    }

    emit captured(image);
}

void CaptureBackend::captureX11() {
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        emit failed("no screens found");
        return;
    }

    QRect unionRect;
    for (QScreen *screen : screens) unionRect = unionRect.united(screen->geometry());

    QImage combined(unionRect.size(), QImage::Format_ARGB32);
    combined.fill(Qt::black);
    QPainter p(&combined);
    for (QScreen *screen : screens) {
        const QPixmap shot = screen->grabWindow(0);
        const QPoint offset = screen->geometry().translated(-unionRect.topLeft()).topLeft();
        p.drawPixmap(offset, shot);
    }
    p.end();

    emit captured(combined);
}
