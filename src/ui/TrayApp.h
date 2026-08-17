#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

#include "services/Config.h"

class QSystemTrayIcon;
class QMenu;
class CaptureBackend;
class HotkeyManager;
class OverlayWindow;

class TrayApp : public QObject {
    Q_OBJECT
public:
    explicit TrayApp(QObject *parent = nullptr);

private slots:
    void startCapture();
    void onCaptured(const QImage &image);
    void onCaptureFailed(const QString &reason);
    void showSettings();
    void showHistory();

private:
    CaptureBackend *backend;
    HotkeyManager *hotkeys;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;

    Config config;
    QStringList history;
    bool captureInProgress = false;
    bool modalDialogOpen = false;

    QVector<OverlayWindow *> *activeOverlays = nullptr;

    void rebuildMenu();
    void loadHistory();
    void recordHistory(const QString &path);
    QString historyFilePath() const;
};
