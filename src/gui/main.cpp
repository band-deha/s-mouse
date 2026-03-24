#include "mainwindow.h"
#include "tray_icon.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("S-Mouse");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("s-mouse");

    // Keep running when main window is closed (tray icon mode)
    app.setQuitOnLastWindowClosed(false);

    smouse::MainWindow window;
    smouse::TrayIcon tray;

    QObject::connect(&tray, &smouse::TrayIcon::show_window_requested, &window, [&window]() {
        window.show();
        window.raise();
        window.activateWindow();
    });

    QObject::connect(&tray, &smouse::TrayIcon::quit_requested, &app, &QApplication::quit);

    window.show();
    tray.show();

    return app.exec();
}
