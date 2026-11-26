#include "MainWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    app.setApplicationName(QStringLiteral("DirectDrop"));
    app.setApplicationVersion(QStringLiteral("1.0.14"));

    directdrop::MainWindow win;
    win.show();
    return app.exec();
}
