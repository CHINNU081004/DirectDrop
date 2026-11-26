#include "MainWindow.hpp"

#include <QApplication>
#include <QDir>
#include <QImageWriter>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    app.setApplicationName(QStringLiteral("DirectDrop"));
    app.setApplicationVersion(QStringLiteral("1.2.1"));

    directdrop::MainWindow win;
    win.show();

    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--screenshot")) {
        QString outDir = QString::fromLocal8Bit(argv[2]);
        QDir().mkpath(outDir);
        QTimer::singleShot(300, [outDir, &win, &app]() {
            const QStringList names = {
                QStringLiteral("/01_desktop_overview.png"),
                QStringLiteral("/02_desktop_files.png"),
                QStringLiteral("/03_desktop_connect.png"),
                QStringLiteral("/04_desktop_transfers.png"),
                QStringLiteral("/05_desktop_storage.png")
            };
            for (int i = 0; i < names.size(); ++i) {
                win.setPageIndex(i);
                QImage img(1040, 680, QImage::Format_ARGB32);
                img.fill(Qt::transparent);
                win.render(&img);
                QImageWriter w(outDir + names.at(i));
                w.write(img);
            }
            app.quit();
        });
    }

    return app.exec();
}
