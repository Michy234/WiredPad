#include "window.hpp"
#include <QApplication>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("WiredPad");
    app.setOrganizationName("WiredPad");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("io.github.Michy234.WiredPad");
    app.setWindowIcon(QIcon(":/assets/icon.svg"));
    app.setQuitOnLastWindowClosed(false);
    QLockFile lock(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
                   "/wiredpad-app.lock");
    if (!lock.tryLock()) {
        QMessageBox::information(nullptr, "WiredPad", "WiredPad is already open.");
        return 0;
    }
    Window window;
    window.show();
    if (app.arguments().contains("--start"))
        window.startWhenReady();
    return app.exec();
}
