#include <QApplication>
#include <QIcon>

#include "ui/TrayApp.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(":/sharexl.png"));

    TrayApp trayApp;

    return app.exec();
}
