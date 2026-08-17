#include "services/HotkeyManager.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QUuid>
#include <cstdio>

namespace {
const char *kPortalService = "org.freedesktop.portal.Desktop";
const char *kPortalPath = "/org/freedesktop/portal/desktop";
const char *kInterface = "org.freedesktop.portal.GlobalShortcuts";

struct ShortcutSpec {
    QString id;
    QVariantMap properties;
};

QDBusArgument &operator<<(QDBusArgument &arg, const ShortcutSpec &s) {
    arg.beginStructure();
    arg << s.id << s.properties;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, ShortcutSpec &s) {
    arg.beginStructure();
    arg >> s.id >> s.properties;
    arg.endStructure();
    return arg;
}

QString newToken() { return "sharexl_" + QUuid::createUuid().toString(QUuid::Id128); }
}

Q_DECLARE_METATYPE(ShortcutSpec)

HotkeyManager::HotkeyManager(QObject *parent) : QObject(parent) {
    qDBusRegisterMetaType<ShortcutSpec>();
    qDBusRegisterMetaType<QList<ShortcutSpec>>();
}

void HotkeyManager::start() {
    QDBusConnection::sessionBus().connect(QString(), QString(), kInterface, "Activated", this,
                                           SLOT(onActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));

    QDBusInterface portal(kPortalService, kPortalPath, kInterface, QDBusConnection::sessionBus());
    if (!portal.isValid()) {
        std::fprintf(stderr, "sharexl: GlobalShortcuts portal not available\n");
        return;
    }

    QVariantMap options;
    options["session_handle_token"] = newToken();

    QDBusReply<QDBusObjectPath> reply = portal.call("CreateSession", options);
    if (!reply.isValid()) {
        std::fprintf(stderr, "sharexl: CreateSession failed: %s\n", qPrintable(reply.error().message()));
        return;
    }

    QDBusConnection::sessionBus().connect(QString(), reply.value().path(), "org.freedesktop.portal.Request",
                                           "Response", this, SLOT(onSessionResponse(uint, QVariantMap)));
}

void HotkeyManager::onSessionResponse(uint code, const QVariantMap &results) {
    if (sessionResponseHandled) return;
    sessionResponseHandled = true;

    if (code != 0) {
        std::fprintf(stderr, "sharexl: shortcut session was not created (code %u)\n", code);
        return;
    }
    const QVariant handleVariant = results.value("session_handle");
    sessionHandle = handleVariant.canConvert<QDBusObjectPath>() ? handleVariant.value<QDBusObjectPath>().path()
                                                                 : handleVariant.toString();
    if (sessionHandle.isEmpty()) {
        std::fprintf(stderr, "sharexl: session_handle missing or unreadable from CreateSession response\n");
        return;
    }

    QDBusInterface portal(kPortalService, kPortalPath, kInterface, QDBusConnection::sessionBus());

    QVariantMap shortcutProps;
    shortcutProps["description"] = "Capture region";
    shortcutProps["preferred_trigger"] = "CTRL+SHIFT+S";

    QList<ShortcutSpec> shortcuts{ShortcutSpec{"capture", shortcutProps}};

    QVariantMap options;
    QDBusReply<QDBusObjectPath> reply =
        portal.call("BindShortcuts", QVariant::fromValue(QDBusObjectPath(sessionHandle)),
                    QVariant::fromValue(shortcuts), QString(), options);
    if (!reply.isValid()) {
        std::fprintf(stderr, "sharexl: BindShortcuts failed: %s\n", qPrintable(reply.error().message()));
        return;
    }

    QDBusConnection::sessionBus().connect(QString(), reply.value().path(), "org.freedesktop.portal.Request",
                                           "Response", this, SLOT(onBindResponse(uint, QVariantMap)));
}

void HotkeyManager::onBindResponse(uint code, const QVariantMap &results) {
    Q_UNUSED(results);
    if (code != 0) {
        std::fprintf(stderr, "sharexl: shortcut binding was not confirmed (code %u)\n", code);
        return;
    }
    std::fprintf(stderr, "sharexl: shortcut bound\n");
}

void HotkeyManager::onActivated(const QDBusObjectPath &activatedSession, const QString &shortcutId,
                                 qulonglong timestamp, const QVariantMap &options) {
    Q_UNUSED(timestamp);
    Q_UNUSED(options);
    if (activatedSession.path() != sessionHandle) return;
    if (shortcutId != "capture") return;
    emit triggered();
}
