#pragma once

#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace directdrop {

struct P2pPeerInfo {
    QString objectPath;
    QString name;
    QString hwAddress;
    QString manufacturer;
    QString model;
    int strength = 0;
};

enum class P2pState {
    Idle,
    Searching,
    DeviceFound,
    NegotiatingGroup,
    ConfiguringIp,
    Connected,
    Failed
};

class WifiP2pBackend : public QObject {
    Q_OBJECT
public:
    explicit WifiP2pBackend(QObject* parent = nullptr);
    ~WifiP2pBackend() override;

    bool initialize(QString* outError = nullptr);
    bool startDiscovery(QString* outError = nullptr);
    bool stopDiscovery(QString* outError = nullptr);
    bool connectToPeer(const QString& peerObjectPath, QString* outError = nullptr);
    void disconnectP2p();

    QList<P2pPeerInfo> peers() const { return m_peers.values(); }
    P2pState state() const { return m_state; }
    QString devicePath() const { return m_p2pDevicePath; }
    QString ipInterface() const { return m_ipInterface; }
    QString localIp() const { return m_localIp; }
    QString gatewayIp() const { return m_gatewayIp; }

signals:
    void stateChanged(P2pState newState, const QString& detail);
    void peerAdded(const P2pPeerInfo& peer);
    void peerRemoved(const QString& objectPath);
    void p2pConnected(const QString& iface, const QString& localIp, const QString& gatewayIp);
    void p2pDisconnected();
    void errorOccurred(const QString& error);

private slots:
    void onPeerAdded(const QDBusObjectPath& path);
    void onPeerRemoved(const QDBusObjectPath& path);
    void onDeviceStateChanged(uint newState, uint oldState, uint reason);

private:
    void setState(P2pState s, const QString& detail = {});
    void queryExistingPeers();
    void fetchPeerProperties(const QString& peerPath);
    void resolveIpConfiguration();

    QString m_p2pDevicePath;
    QString m_activeConnectionPath;
    QString m_ipInterface;
    QString m_localIp;
    QString m_gatewayIp;
    P2pState m_state = P2pState::Idle;
    QMap<QString, P2pPeerInfo> m_peers;
};

} // namespace directdrop
