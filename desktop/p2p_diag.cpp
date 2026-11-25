#include "WifiP2pBackend.hpp"
#include "Protocol.hpp"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    directdrop::WifiP2pBackend backend;
    QString err;
    if (!backend.initialize(&err)) {
        out << "ERROR: Failed to initialize Wi-Fi P2P backend: " << err << "\n";
        return 1;
    }
    out << "Initialized P2P device: " << backend.devicePath() << "\n";

    QObject::connect(&backend, &directdrop::WifiP2pBackend::peerAdded, [&](const directdrop::P2pPeerInfo& peer) {
        out << "[PEER FOUND] " << peer.name << " (" << peer.hwAddress << ") signal=" << peer.strength << "%\n";
        out.flush();
    });

    QObject::connect(&backend, &directdrop::WifiP2pBackend::stateChanged, [&](directdrop::P2pState, const QString& detail) {
        out << "[STATE] " << detail << "\n";
        out.flush();
    });

    if (!backend.startDiscovery(&err)) {
        out << "ERROR: StartFind failed: " << err << "\n";
        return 1;
    }

    int timeoutSec = 5;
    if (argc > 1) {
        timeoutSec = QString::fromLocal8Bit(argv[1]).toInt();
        if (timeoutSec <= 0) timeoutSec = 5;
    }
    out << "Scanning for " << timeoutSec << " seconds...\n";
    out.flush();

    QTimer::singleShot(timeoutSec * 1000, &app, [&]() {
        backend.stopDiscovery();
        out << "Scan complete. Found " << backend.peers().size() << " peer(s).\n";
        app.quit();
    });

    return app.exec();
}
