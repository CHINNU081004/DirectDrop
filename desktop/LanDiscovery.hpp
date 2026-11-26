#pragma once

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QtGlobal>

namespace directdrop {

struct LanPeerInfo {
    QString deviceName;
    QString ip;
    quint16 tlsPort = 0;
    QString bands;
    qint64 lastSeenMs = 0;
};

/**
 * Lightweight local-network peer discovery over UDP (port 47292).
 * Broadcasts only non-sensitive display name, TLS port, and supported band summary
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

    void start(const QString& deviceName, quint16 tlsPort, const QString& bandSummary) {
        m_deviceName = deviceName;
        m_tlsPort = tlsPort;
        m_bandSummary = bandSummary;
        if (!m_beaconTimer->isActive()) m_beaconTimer->start();
        sendBeacon();
    }

    void stop() {
        m_beaconTimer->stop();
    }

    QList<LanPeerInfo> peers() const { return m_peers.values(); }

signals:
    void peersUpdated(const QList<LanPeerInfo>& peers);

private slots:
    void sendBeacon() {
        if (m_tlsPort == 0) return;
        QJsonObject obj;
        obj[QStringLiteral("app")] = QStringLiteral("DirectDrop");
        obj[QStringLiteral("proto")] = 1;
        obj[QStringLiteral("name")] = m_deviceName;
        obj[QStringLiteral("port")] = static_cast<int>(m_tlsPort);
        obj[QStringLiteral("bands")] = m_bandSummary;
        QByteArray datagram = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        m_sock->writeDatagram(datagram, QHostAddress::Broadcast, kDiscoveryPort);
    }

    void onReadyRead() {
        while (m_sock->hasPendingDatagrams()) {
            QByteArray data;
            data.resize(static_cast<int>(m_sock->pendingDatagramSize()));
            QHostAddress sender;
            m_sock->readDatagram(data.data(), data.size(), &sender);
            if (data.size() > 1024) continue;
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isObject()) continue;
            QJsonObject obj = doc.object();
            if (obj.value(QStringLiteral("app")).toString() != QStringLiteral("DirectDrop")) continue;
            int port = obj.value(QStringLiteral("port")).toInt();
            if (port <= 0 || port > 65535) continue;

            QString ip = sender.toString();
            if (ip.startsWith(QStringLiteral("::ffff:"))) ip = ip.mid(7);
            if (ip.isEmpty() || ip == QStringLiteral("127.0.0.1")) continue;

            LanPeerInfo info;
            info.deviceName = obj.value(QStringLiteral("name")).toString().left(64);
            info.ip = ip;
            info.tlsPort = static_cast<quint16>(port);
            info.bands = obj.value(QStringLiteral("bands")).toString().left(48);
            m_peers.insert(ip, info);
            emit peersUpdated(m_peers.values());
        }
    }

private:
    QUdpSocket* m_sock = nullptr;
    QTimer* m_beaconTimer = nullptr;
    QString m_deviceName;
    quint16 m_tlsPort = 0;
    QString m_bandSummary;
    QMap<QString, LanPeerInfo> m_peers;
};

} // namespace directdrop
