#pragma once

#include "AdaptiveWifiPolicy.hpp"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

namespace directdrop {

struct LanPeerInfo {
    QString deviceName;
    QString ip;
    quint16 tlsPort = 0;
    QString bands;
    PeerCapabilities caps;
};

/**
 * Local-network UDP peer discovery (port 47292) + TCP reachability probe.
 * Exchanges only non-sensitive display name, TLS port, and capability states
 * (never IMEI, serial numbers, or permanent MAC addresses).
 */
class LanDiscoveryService : public QObject {
    Q_OBJECT
public:
    static constexpr quint16 kDiscoveryPort = 47292;

    explicit LanDiscoveryService(QObject* parent = nullptr) : QObject(parent) {
        m_sock = new QUdpSocket(this);
        m_sock->bind(QHostAddress::AnyIPv4, kDiscoveryPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
        connect(m_sock, &QUdpSocket::readyRead, this, &LanDiscoveryService::onReadyRead);

        m_beaconTimer = new QTimer(this);
        m_beaconTimer->setInterval(3000);
        connect(m_beaconTimer, &QTimer::timeout, this, &LanDiscoveryService::sendBeacon);
    }

    void start(const QString& deviceName, quint16 tlsPort, const AdapterCapabilities& localCap) {
        m_deviceName = deviceName;
        m_tlsPort = tlsPort;
        m_localCap = localCap;
        if (!m_beaconTimer->isActive()) m_beaconTimer->start();
        sendBeacon();
    }

    void stop() { m_beaconTimer->stop(); }
    QList<LanPeerInfo> peers() const { return m_peers.values(); }

    static ReachabilityState probeTcpReachability(const QString& ip, quint16 port, int timeoutMs = 1500) {
        QTcpSocket sock;
        sock.connectToHost(ip, port);
        if (sock.waitForConnected(timeoutMs)) {
            sock.disconnectFromHost();
            return ReachabilityState::Reachable;
        }
        if (sock.error() == QAbstractSocket::SocketTimeoutError) {
            return ReachabilityState::ClientIsolationSuspected;
        }
        return ReachabilityState::Unreachable;
    }

signals:
    void peersUpdated(const QList<LanPeerInfo>& peers);

private slots:
    void sendBeacon() {
        if (m_tlsPort == 0) return;
        QJsonObject obj;
        obj[QStringLiteral("app")] = QStringLiteral("DirectDrop");
        obj[QStringLiteral("proto")] = 2;
        obj[QStringLiteral("name")] = m_deviceName;
        obj[QStringLiteral("port")] = static_cast<int>(m_tlsPort);
        obj[QStringLiteral("bands")] = m_localCap.summary;
        obj[QStringLiteral("w24")] = static_cast<int>(m_localCap.wifi.band24);
        obj[QStringLiteral("w5")] = static_cast<int>(m_localCap.wifi.band5);
        obj[QStringLiteral("w6")] = static_cast<int>(m_localCap.wifi.band6);
        obj[QStringLiteral("p2p")] = static_cast<int>(m_localCap.wifiDirect.available);
        obj[QStringLiteral("freq")] = m_localCap.routerLan.activeFrequencyMhz;
        QByteArray datagram = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        m_sock->writeDatagram(datagram, QHostAddress::Broadcast, kDiscoveryPort);
    }

    void onReadyRead() {
        while (m_sock->hasPendingDatagrams()) {
            QByteArray data;
            data.resize(static_cast<int>(m_sock->pendingDatagramSize()));
            QHostAddress sender;
            m_sock->readDatagram(data.data(), data.size(), &sender);
            if (data.size() > 1536) continue;
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isObject()) continue;
            QJsonObject obj = doc.object();
            if (obj.value(QStringLiteral("app")).toString() != QStringLiteral("DirectDrop")) continue;
            int port = obj.value(QStringLiteral("port")).toInt();
            if (port <= 0 || port > 65535) continue;

            QString ip = sender.toString();
            if (ip.startsWith(QStringLiteral("::ffff:"))) ip = ip.mid(7);
            if (ip.isEmpty() || ip == QStringLiteral("127.0.0.1") ||
                m_localCap.routerLan.lanAddresses.contains(ip)) continue;

            auto toCap = [](int v) -> CapabilityState {
                return (v >= 0 && v <= 3) ? static_cast<CapabilityState>(v) : CapabilityState::Unknown;
            };

            LanPeerInfo info;
            info.deviceName = obj.value(QStringLiteral("name")).toString().left(64);
            info.ip = ip;
            info.tlsPort = static_cast<quint16>(port);
            info.bands = obj.value(QStringLiteral("bands")).toString().left(64);
            info.caps.name = info.deviceName;
            info.caps.known = obj.contains(QStringLiteral("w24"));
            info.caps.wifi.band24 = toCap(obj.value(QStringLiteral("w24")).toInt(3));
            info.caps.wifi.band5 = toCap(obj.value(QStringLiteral("w5")).toInt(3));
            info.caps.wifi.band6 = toCap(obj.value(QStringLiteral("w6")).toInt(3));
            info.caps.wifiDirectAvailable = toCap(obj.value(QStringLiteral("p2p")).toInt(3));
            info.caps.routerLanAvailable = CapabilityState::Supported;
            info.caps.activeFrequencyMhz = obj.value(QStringLiteral("freq")).toInt(0);
            m_peers.insert(ip, info);
            emit peersUpdated(m_peers.values());
        }
    }

private:
    QUdpSocket* m_sock = nullptr;
    QTimer* m_beaconTimer = nullptr;
    QString m_deviceName;
    quint16 m_tlsPort = 0;
    AdapterCapabilities m_localCap;
    QMap<QString, LanPeerInfo> m_peers;
};

} // namespace directdrop
