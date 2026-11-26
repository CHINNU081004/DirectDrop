#include "MainWindow.hpp"

#include <QApplication>
#include <QDir>
#include <QImageWriter>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    app.setApplicationName(QStringLiteral("DirectDrop"));
    app.setApplicationVersion(QStringLiteral("1.2.0"));

    directdrop::MainWindow win;
    win.show();

    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--screenshot")) {
        QString outDir = QString::fromLocal8Bit(argv[2]);
        QDir().mkpath(outDir);
        QTimer::singleShot(300, [outDir, &win, &app]() {
            win.setPageIndex(0);
            QImage p1(880, 680, QImage::Format_ARGB32);
            p1.fill(Qt::transparent);
            win.render(&p1);
            QImageWriter w1(outDir + QStringLiteral("/05_desktop_setup.png"));
            bool ok1 = w1.write(p1);
            if (!ok1) qDebug("w1 error: %s", qPrintable(w1.errorString()));

            win.setPageIndex(1);
            QImage p2(880, 680, QImage::Format_ARGB32);
            p2.fill(Qt::transparent);
            win.render(&p2);
            QImageWriter w2(outDir + QStringLiteral("/06_desktop_transfer.png"));
            bool ok2 = w2.write(p2);

            win.setPageIndex(2);
            QImage p3(880, 680, QImage::Format_ARGB32);
            p3.fill(Qt::transparent);
            win.render(&p3);
            QImageWriter w3(outDir + QStringLiteral("/07_desktop_connect.png"));
            bool ok3 = w3.write(p3);
            qDebug("Saved desktop screenshots: %d %d %d", ok1, ok2, ok3);
            app.quit();
        });
    }

    return app.exec();
}
