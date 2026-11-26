#include "MainWindow.hpp"
#include "LiquidSidebar.hpp"

#include <QApplication>
#include <QDir>
#include <QImageWriter>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    app.setApplicationName(QStringLiteral("DirectDrop"));
    app.setApplicationVersion(QStringLiteral("1.2.2"));

    directdrop::MainWindow win;
    win.show();

    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--screenshot")) {
        QString outDir = QString::fromLocal8Bit(argv[2]);
        QDir().mkpath(outDir);
        QTimer::singleShot(250, [outDir, &win, &app]() {
            auto saveSnap = [outDir, &win](const QString& name) {
                QImage img(1040, 680, QImage::Format_ARGB32);
                img.fill(Qt::transparent);
                win.render(&img);
                QImageWriter(outDir + name).write(img);
            };

            auto* sb = qobject_cast<directdrop::LiquidSidebar*>(win.sidebarWidget());
            win.setPageIndex(0);
            saveSnap(QStringLiteral("/01_desktop_no_hover.png"));

            if (sb) {
                sb->setHoveredIndex(2, 6.0);
                QTimer::singleShot(220, [saveSnap, sb, &win, &app]() {
                    saveSnap(QStringLiteral("/02_desktop_hover_connect.png"));
                    sb->setHoveredIndex(3, 6.0);
                    QTimer::singleShot(220, [saveSnap, sb, &win, &app]() {
                        saveSnap(QStringLiteral("/03_desktop_hover_transfers.png"));
                        sb->setHoveredIndex(-1);
                        win.setPageIndex(3);
                        QTimer::singleShot(260, [saveSnap, &app]() {
                            saveSnap(QStringLiteral("/04_desktop_transfers.png"));
                            app.quit();
                        });
                    });
                });
            } else {
                app.quit();
            }
        });
    }

    return app.exec();
}
