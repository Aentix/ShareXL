#include "ui/TrayApp.h"

#ifdef HAVE_LAYERSHELLQT
#include <LayerShellQt/Window>
#endif

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenu>
#include <QScreen>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>
#include <QWindow>
#include <cstdio>

#include "capture/CaptureBackend.h"
#include "ui/HistoryDialog.h"
#include "services/HotkeyManager.h"
#include "capture/OverlayWindow.h"
#include "capture/SessionState.h"
#include "services/Platform.h"
#include "ui/SettingsDialog.h"

namespace {
#ifdef HAVE_LAYERSHELLQT
void placeOnScreenLayerShell(OverlayWindow *overlay, QScreen *screen) {
    overlay->winId();
    QWindow *handle = overlay->windowHandle();
    handle->setScreen(screen);

    auto *layerWindow = LayerShellQt::Window::get(handle);
    layerWindow->setAnchors(LayerShellQt::Window::Anchors(
        LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom |
        LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
    layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    layerWindow->setExclusiveZone(-1);
    layerWindow->setScope(QStringLiteral("sharexl"));

    overlay->resize(screen->geometry().size());
}
#endif

void placeOnScreenWaylandFullscreen(OverlayWindow *overlay, QScreen *screen) {
    overlay->winId();
    overlay->windowHandle()->setScreen(screen);
    overlay->resize(screen->geometry().size());
    overlay->showFullScreen();
}

void placeOnScreenX11(OverlayWindow *overlay, QScreen *screen) {
    overlay->setGeometry(screen->geometry());
}

void placeOnScreen(OverlayWindow *overlay, QScreen *screen) {
    if (Platform::isX11()) {
        placeOnScreenX11(overlay, screen);
    }
#ifdef HAVE_LAYERSHELLQT
    else if (Platform::isKdePlasma()) {
        placeOnScreenLayerShell(overlay, screen);
    }
#endif
    else {
        placeOnScreenWaylandFullscreen(overlay, screen);
    }
}
}

TrayApp::TrayApp(QObject *parent) : QObject(parent) {
    config = Config::load();
    loadHistory();

    backend = new CaptureBackend(this);
    connect(backend, &CaptureBackend::captured, this, &TrayApp::onCaptured);
    connect(backend, &CaptureBackend::failed, this, &TrayApp::onCaptureFailed);

    hotkeys = new HotkeyManager(this);
    connect(hotkeys, &HotkeyManager::triggered, this, &TrayApp::startCapture);
    hotkeys->start();

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/sharexl.png"));
    trayIcon->setToolTip("ShareXL");

    trayMenu = new QMenu();
    rebuildMenu();
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) startCapture();
    });
}

void TrayApp::rebuildMenu() {
    trayMenu->clear();

    QAction *captureAction = trayMenu->addAction("Capture Region");
    connect(captureAction, &QAction::triggered, this, &TrayApp::startCapture);

    trayMenu->addSeparator();

    QAction *historyAction = trayMenu->addAction("Recent Captures...");
    connect(historyAction, &QAction::triggered, this, &TrayApp::showHistory);

    trayMenu->addSeparator();
    QAction *settingsAction = trayMenu->addAction("Settings...");
    connect(settingsAction, &QAction::triggered, this, &TrayApp::showSettings);

    trayMenu->addSeparator();
    QAction *quitAction = trayMenu->addAction("Quit");
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
}

void TrayApp::startCapture() {
    if (captureInProgress || modalDialogOpen) return;
    captureInProgress = true;
    backend->requestScreenshot();
}

void TrayApp::onCaptureFailed(const QString &reason) {
    captureInProgress = false;
    std::fprintf(stderr, "sharexl: capture failed: %s\n", qPrintable(reason));
}

void TrayApp::onCaptured(const QImage &image) {
    const auto screens = QGuiApplication::screens();

    QRect unionRect;
    for (QScreen *screen : screens) unionRect = unionRect.united(screen->geometry());

    auto *session = new SessionState(this);
    session->fullImage = image;
    session->drawColor = config.drawColor;
    session->drawThickness = config.drawThickness;
    session->textFont = QFont(config.textFontFamily, config.textFontSize, QFont::Bold);
    session->textBackground = config.textBackground;
    session->saveDirectory = config.saveDirectory;

    activeOverlays = new QVector<OverlayWindow *>();

    bool first = true;
    for (QScreen *screen : screens) {
        const QPoint offset = screen->geometry().translated(-unionRect.topLeft()).topLeft();
        auto *overlay = new OverlayWindow(session, offset, first);
        placeOnScreen(overlay, screen);
        activeOverlays->append(overlay);
        overlay->show();
        first = false;
    }

    connect(session, &SessionState::savedTo, this, &TrayApp::recordHistory);
    connect(session, &SessionState::ended, this, [this, session](bool copied) {
        Q_UNUSED(copied);
        config.drawColor = session->drawColor;
        config.drawThickness = session->drawThickness;
        config.textFontFamily = session->textFont.family();
        config.textFontSize = session->textFont.pointSize();
        config.textBackground = session->textBackground;
        config.save();

        if (activeOverlays) {
            for (OverlayWindow *w : *activeOverlays) w->close();
            delete activeOverlays;
            activeOverlays = nullptr;
        }
        captureInProgress = false;
        session->deleteLater();
    });
}

void TrayApp::showSettings() {
    modalDialogOpen = true;
    SettingsDialog dialog(config);
    if (dialog.exec() == QDialog::Accepted) {
        config = dialog.config();
        config.save();
    }
    modalDialogOpen = false;
}

void TrayApp::showHistory() {
    modalDialogOpen = true;
    HistoryDialog dialog(history, [this] { QTimer::singleShot(0, this, &TrayApp::showHistory); });
    dialog.exec();
    modalDialogOpen = false;
}

void TrayApp::recordHistory(const QString &path) {
    history.prepend(path);
    while (history.size() > 10) history.removeLast();

    QFile file(historyFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        for (const QString &p : history) out << p << '\n';
    }
}

void TrayApp::loadHistory() {
    QFile file(historyFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty() && QFileInfo::exists(line)) history.append(line);
    }
}

QString TrayApp::historyFilePath() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/sharexl";
    QDir().mkpath(dir);
    return dir + "/history.txt";
}
