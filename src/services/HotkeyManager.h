#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QDBusObjectPath>

class HotkeyManager : public QObject {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject *parent = nullptr);
    void start();

signals:
    void triggered();

private slots:
    void onSessionResponse(uint code, const QVariantMap &results);
    void onBindResponse(uint code, const QVariantMap &results);
    void onActivated(const QDBusObjectPath &sessionHandle, const QString &shortcutId, qulonglong timestamp,
                      const QVariantMap &options);

private:
    QString sessionHandle;
    bool sessionResponseHandled = false;
};
