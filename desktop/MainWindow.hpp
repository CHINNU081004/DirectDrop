#pragma once

#include "TlsServer.hpp"
#include "WifiP2pBackend.hpp"

#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>

namespace directdrop {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onToggleReceiver();
    void onStartScan();
    void onConnectSelectedPeer();
    void onChooseDestination();
    void onOpenDestination();
    void onPeerAdded(const P2pPeerInfo& peer);
    void onP2pStateChanged(P2pState state, const QString& detail);
    void onP2pConnected(const QString& iface, const QString& localIp, const QString& gatewayIp);
    void onPairingRequested(const QString& deviceName, const QString& deviceId, const QString& sasCode);
    void onTransferProgress(const TransferStats& stats);
    void onJobCompleted(const TransferStats& stats);
    void appendLog(const QString& msg);

private:
    void updateDiskSpaceLabel();

    WifiP2pBackend* m_p2p = nullptr;
    TlsReceiverServer* m_server = nullptr;

    QPushButton* m_btnReceiver = nullptr;
    QPushButton* m_btnScan = nullptr;
    QPushButton* m_btnConnectPeer = nullptr;
    QPushButton* m_btnDest = nullptr;
    QPushButton* m_btnOpenDest = nullptr;
    QComboBox* m_conflictCombo = nullptr;

    QLabel* m_lblTransport = nullptr;
    QLabel* m_lblP2pStatus = nullptr;
    QLabel* m_lblDestPath = nullptr;
    QLabel* m_lblDiskSpace = nullptr;
    QLabel* m_lblCurrentFile = nullptr;
    QLabel* m_lblSpeed = nullptr;
    QLabel* m_lblCounts = nullptr;

    QListWidget* m_peerList = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPlainTextEdit* m_logView = nullptr;

    bool m_receiverRunning = false;
};

} // namespace directdrop
