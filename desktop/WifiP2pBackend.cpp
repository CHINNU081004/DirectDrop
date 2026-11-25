#include "WifiP2pBackend.hpp"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QTimer>

namespace directdrop {

static constexpr const char* kNmService = "org.freedesktop.NetworkManager";
static constexpr const char* kNmPath = "/org/freedesktop/NetworkManager";
static constexpr const char* kNmIface = "org.freedesktop.NetworkManager";
static constexpr const char* kNmDevIface = "org.freedesktop.NetworkManager.Device";
static constexpr const char* kNmP2pIface = "org.freedesktop.NetworkManager.Device.WifiP2P";
static constexpr const char* kNmPeerIface = "org.freedesktop.NetworkManager.WifiP2PPeer";
static constexpr const char* kDbusPropsIface = "org.freedesktop.DBus.Properties";

WifiP2pBackend::WifiP2pBackend(QObject* parent) : QObject(parent) {}

WifiP2pBackend::~WifiP2pBackend() {
    stopDiscovery();
}

bool WifiP2pBackend::initialize(QString* outError) {
    auto bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        if (outError) *outError = QStringLiteral("System D-Bus is not connected");
        return false;
    }

    QDBusInterface nm(kNmService, kNmPath, kNmIface, bus);
    if (!nm.isValid()) {
        if (outError) *outError = QStringLiteral("NetworkManager D-Bus service unavailable");
        return false;
    }

    QDBusReply<QList<QDBusObjectPath>> reply = nm.call(QStringLiteral("GetDevices"));
    if (!reply.isValid()) {
        if (outError) *outError = QStringLiteral("GetDevices failed: %1").arg(reply.error().message());
        return false;
    }

    for (const auto& devPath : reply.value()) {
        QDBusInterface props(kNmService, devPath.path(), kDbusPropsIface, bus);
        QDBusReply<QDBusVariant> typeReply = props.call(QStringLiteral("Get"), QString::fromLatin1(kNmDevIface), QStringLiteral("DeviceType"));
        // NM_DEVICE_TYPE_WIFI_P2P = 30
        if (typeReply.isValid() && typeReply.value().variant().toUInt() == 30) {
            m_p2pDevicePath = devPath.path();
            break;
        }
    }

    if (m_p2pDevicePath.isEmpty()) {
        if (outError) *outError = QStringLiteral("No NetworkManager Wi-Fi P2P device found");
        return false;
    }

    bus.connect(kNmService, m_p2pDevicePath, kNmP2pIface, QStringLiteral("PeerAdded"),
                this, SLOT(onPeerAdded(QDBusObjectPath)));
    bus.connect(kNmService, m_p2pDevicePath, kNmP2pIface, QStringLiteral("PeerRemoved"),
                this, SLOT(onPeerRemoved(QDBusObjectPath)));
    bus.connect(kNmService, m_p2pDevicePath, kNmDevIface, QStringLiteral("StateChanged"),
                this, SLOT(onDeviceStateChanged(uint,uint,uint)));

    queryExistingPeers();
    return true;
}

bool WifiP2pBackend::startDiscovery(QString* outError) {
    if (m_p2pDevicePath.isEmpty()) {
        if (outError) *outError = QStringLiteral("Backend not initialized");
        return false;
    }
    QDBusInterface p2p(kNmService, m_p2pDevicePath, kNmP2pIface, QDBusConnection::systemBus());
    QVariantMap opts;
    QDBusReply<void> reply = p2p.call(QStringLiteral("StartFind"), opts);
    if (!reply.isValid()) {
        if (outError) *outError = QStringLiteral("StartFind failed: %1").arg(reply.error().message());
        return false;
    }
    setState(P2pState::Searching, QStringLiteral("Searching for Wi-Fi Direct peers"));
    return true;
}

bool WifiP2pBackend::stopDiscovery(QString* outError) {
    if (m_p2pDevicePath.isEmpty()) return true;
    QDBusInterface p2p(kNmService, m_p2pDevicePath, kNmP2pIface, QDBusConnection::systemBus());
    QDBusReply<void> reply = p2p.call(QStringLiteral("StopFind"));
    if (!reply.isValid()) {
        if (outError) *outError = QStringLiteral("StopFind failed: %1").arg(reply.error().message());
        return false;
    }
    if (m_state == P2pState::Searching) {
        setState(P2pState::Idle, QStringLiteral("Discovery stopped"));
    }
    return true;
}

bool WifiP2pBackend::connectToPeer(const QString& peerObjectPath, QString* outError) {
    if (m_p2pDevicePath.isEmpty()) {
        if (outError) *outError = QStringLiteral("Backend not initialized");
        return false;
    }
    setState(P2pState::NegotiatingGroup, QStringLiteral("Negotiating Wi-Fi Direct group"));

    QVariantMap connSection;
    connSection[QStringLiteral("type")] = QStringLiteral("wifi-p2p");
    connSection[QStringLiteral("id")] = QStringLiteral("DirectDrop-P2P");

    QString peerHwAddr = m_peers.value(peerObjectPath).hwAddress;
    if (peerHwAddr.isEmpty()) {
        if (outError) *outError = QStringLiteral("Peer hardware address not found");
        return false;
    }

    QVariantMap p2pSection;
    p2pSection[QStringLiteral("peer")] = peerHwAddr;

    QMap<QString, QVariantMap> settings;
    settings[QStringLiteral("connection")] = connSection;
    settings[QStringLiteral("wifi-p2p")] = p2pSection;

    QDBusInterface nm(kNmService, kNmPath, kNmIface, QDBusConnection::systemBus());
    QDBusMessage msg = nm.call(QStringLiteral("AddAndActivateConnection"),
                               QVariant::fromValue(settings),
                               QVariant::fromValue(QDBusObjectPath(m_p2pDevicePath)),
                               QVariant::fromValue(QDBusObjectPath(QStringLiteral("/"))));
    if (msg.type() == QDBusMessage::ErrorMessage) {
        setState(P2pState::Failed, msg.errorMessage());
        if (outError) *outError = msg.errorMessage();
        return false;
    }
    if (msg.arguments().size() >= 2) {
        m_activeConnectionPath = msg.arguments().at(1).value<QDBusObjectPath>().path();
    }
    return true;
}

void WifiP2pBackend::disconnectP2p() {
    if (!m_p2pDevicePath.isEmpty()) {
        QDBusInterface dev(kNmService, m_p2pDevicePath, kNmDevIface, QDBusConnection::systemBus());
        dev.call(QStringLiteral("Disconnect"));
    }
    m_ipInterface.clear();
    m_localIp.clear();
    m_gatewayIp.clear();
    setState(P2pState::Idle, QStringLiteral("Disconnected"));
    emit p2pDisconnected();
}

void WifiP2pBackend::setState(P2pState s, const QString& detail) {
    m_state = s;
    emit stateChanged(s, detail);
}

void WifiP2pBackend::queryExistingPeers() {
    QDBusInterface props(kNmService, m_p2pDevicePath, kDbusPropsIface, QDBusConnection::systemBus());
    QDBusReply<QDBusVariant> reply = props.call(QStringLiteral("Get"), QString::fromLatin1(kNmP2pIface), QStringLiteral("Peers"));
    if (!reply.isValid()) return;
    const auto paths = qdbus_cast<QList<QDBusObjectPath>>(reply.value().variant());
    for (const auto& p : paths) {
        fetchPeerProperties(p.path());
    }
}

void WifiP2pBackend::onPeerAdded(const QDBusObjectPath& path) {
    fetchPeerProperties(path.path());
}

void WifiP2pBackend::onPeerRemoved(const QDBusObjectPath& path) {
    m_peers.remove(path.path());
    emit peerRemoved(path.path());
}

void WifiP2pBackend::fetchPeerProperties(const QString& peerPath) {
    QDBusInterface props(kNmService, peerPath, kDbusPropsIface, QDBusConnection::systemBus());
    QDBusReply<QVariantMap> reply = props.call(QStringLiteral("GetAll"), QString::fromLatin1(kNmPeerIface));
    if (!reply.isValid()) return;

    const QVariantMap map = reply.value();
    P2pPeerInfo info;
    info.objectPath = peerPath;
    info.name = map.value(QStringLiteral("Name")).toString();
    info.hwAddress = map.value(QStringLiteral("HwAddress")).toString();
    info.manufacturer = map.value(QStringLiteral("Manufacturer")).toString();
    info.model = map.value(QStringLiteral("Model")).toString();
    info.strength = map.value(QStringLiteral("Strength")).toInt();

    m_peers[peerPath] = info;
    if (m_state == P2pState::Searching) {
        setState(P2pState::DeviceFound, QStringLiteral("Found peer: %1").arg(info.name));
    }
    emit peerAdded(info);
}

void WifiP2pBackend::onDeviceStateChanged(uint newState, uint /*oldState*/, uint reason) {
    // NM_DEVICE_STATE_IP_CONFIG = 70, NM_DEVICE_STATE_ACTIVATED = 100, NM_DEVICE_STATE_FAILED = 120
    if (newState == 70) {
        setState(P2pState::ConfiguringIp, QStringLiteral("Configuring P2P IP address"));
    } else if (newState == 100) {
        resolveIpConfiguration();
    } else if (newState == 120) {
        setState(P2pState::Failed, QStringLiteral("P2P connection failed (reason %1)").arg(reason));
        emit errorOccurred(QStringLiteral("P2P connection failed (reason %1)").arg(reason));
    } else if (newState == 30) {
        m_ipInterface.clear();
        m_localIp.clear();
        m_gatewayIp.clear();
        setState(P2pState::Idle, QStringLiteral("Disconnected"));
        emit p2pDisconnected();
    }
}

void WifiP2pBackend::resolveIpConfiguration() {
    auto bus = QDBusConnection::systemBus();
    QDBusInterface devProps(kNmService, m_p2pDevicePath, kDbusPropsIface, bus);
    QDBusReply<QDBusVariant> ifaceReply = devProps.call(QStringLiteral("Get"), QString::fromLatin1(kNmDevIface), QStringLiteral("IpInterface"));
    if (ifaceReply.isValid()) {
        m_ipInterface = ifaceReply.value().variant().toString();
    }

    QDBusReply<QDBusVariant> ip4PathReply = devProps.call(QStringLiteral("Get"), QString::fromLatin1(kNmDevIface), QStringLiteral("Ip4Config"));
    if (ip4PathReply.isValid()) {
        QString ip4Path = ip4PathReply.value().variant().value<QDBusObjectPath>().path();
        if (!ip4Path.isEmpty() && ip4Path != QStringLiteral("/")) {
            QDBusInterface ip4Props(kNmService, ip4Path, kDbusPropsIface, bus);
            QDBusReply<QDBusVariant> gwReply = ip4Props.call(QStringLiteral("Get"), QStringLiteral("org.freedesktop.NetworkManager.IP4Config"), QStringLiteral("Gateway"));
            if (gwReply.isValid()) {
                m_gatewayIp = gwReply.value().variant().toString();
            }
            QDBusReply<QDBusVariant> addrReply = ip4Props.call(QStringLiteral("Get"), QStringLiteral("org.freedesktop.NetworkManager.IP4Config"), QStringLiteral("AddressData"));
            if (addrReply.isValid()) {
                const auto list = qdbus_cast<QList<QVariantMap>>(addrReply.value().variant());
                if (!list.isEmpty()) {
                    m_localIp = list.first().value(QStringLiteral("address")).toString();
                }
            }
        }
    }

    setState(P2pState::Connected, QStringLiteral("Connected on %1 (%2)").arg(m_ipInterface, m_localIp));
    emit p2pConnected(m_ipInterface, m_localIp, m_gatewayIp);
}

} // namespace directdrop
