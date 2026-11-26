#pragma once

#include "AdaptiveWifiPolicy.hpp"
#include "FileManagerWidget.hpp"
#include "LanDiscovery.hpp"
#include "TlsServer.hpp"
#include "WifiP2pBackend.hpp"

#include <QComboBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>

namespace directdrop {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;
    void setPageIndex(int idx);
    QWidget* sidebarWidget() const { return m_sidebar; }

private slots:
    void onStartScan();
    void onConnectSelectedPeer();
    void onChooseDestination();
    void onOpenDestination();
    void onQueueFilesToSend();
    void onQueueFolderToSend();
    void onSendQueuedToPeer();
    void onPeerAdded(const P2pPeerInfo& peer);
    void onP2pStateChanged(P2pState state, const QString& detail);
    void onP2pConnected(const QString& iface, const QString& localIp, const QString& gatewayIp);
    void onPairingRequested(const QString& deviceName, const QString& deviceId, const QString& sasCode);
    void onTransferStarted(int totalFiles, uint64_t totalBytes);
    void onFileProgress(const QString& relPath, uint64_t receivedBytes, uint64_t totalBytes, bool verified);
    void onTransferProgress(const TransferStats& stats);
    void onJobCompleted(const TransferStats& stats);
    void appendLog(const QString& msg);

private:
    void updateDiskSpaceLabel();
    void applyBandPreference(int idx, bool persist);

    WifiP2pBackend* m_p2p = nullptr;
    TlsReceiverServer* m_server = nullptr;
    LanDiscoveryService* m_lanDiscovery = nullptr;
    AdapterCapabilities m_adapterCap;
    TransportKind m_activeTransport = TransportKind::WifiDirectP2p;
    QString m_lanTargetIp;
    quint16 m_lanTargetPort = 47291;

    QStackedWidget* m_pages = nullptr;
    QWidget* m_sidebar = nullptr;

    QPushButton* m_btnTabShare = nullptr;
    QPushButton* m_btnTabHistory = nullptr;
    FileManagerWidget* m_fileManager = nullptr;
    QLabel* m_lblAdapterCap = nullptr;
    QLabel* m_lblBandPolicy = nullptr;
    QPushButton* m_btnOpenConnectPage = nullptr;
    QPushButton* m_btnBackFromConnect = nullptr;

    QPushButton* m_btnScan = nullptr;
    QPushButton* m_btnConnectPeer = nullptr;
    QPushButton* m_btnDest = nullptr;
    QPushButton* m_btnOpenDest = nullptr;
    QPushButton* m_btnQueueFiles = nullptr;
    QPushButton* m_btnQueueFolder = nullptr;
    QPushButton* m_btnSendQueued = nullptr;
    QPushButton* m_btnBackToSetup = nullptr;
    QWidget* m_radarWidget = nullptr;
    QWidget* m_bandSelector = nullptr;
    QWidget* m_connectBandSelector = nullptr;
    QLabel* m_lblConnBandStatus = nullptr;
    QLabel* m_lblConnWhy = nullptr;
    QLineEdit* m_lanIpEdit = nullptr;
    QPushButton* m_btnConnectLan = nullptr;
    QListWidget* m_lanPeerList = nullptr;
    QWidget* m_deviceCardsHost = nullptr;
    QLabel* m_lblSendQueue = nullptr;
    QStringList m_queuedSendFiles;
    QString m_queuedSendBaseDir;
    QComboBox* m_conflictCombo = nullptr;

    QLabel* m_lblTransport = nullptr;
    QLabel* m_lblP2pStatus = nullptr;
    QLabel* m_lblDestPath = nullptr;
    QLabel* m_lblDiskSpace = nullptr;
    QLabel* m_lblCurrentFile = nullptr;
    QLabel* m_lblSpeed = nullptr;
    QLabel* m_lblCounts = nullptr;

    QListWidget* m_peerList = nullptr;
    QListWidget* m_transferFileList = nullptr;
    QHash<QString, QListWidgetItem*> m_fileItems;
    QProgressBar* m_progressBar = nullptr;
    QPlainTextEdit* m_logView = nullptr;

    bool m_receiverRunning = false;
};

} // namespace directdrop
