#include "MainWindow.hpp"
#include "LiquidDropProgressBar.hpp"
#include "RadarScanWidget.hpp"
#include "TlsClient.hpp"

#include <QDesktopServices>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QStorageInfo>
#include <QUrl>
#include <QVBoxLayout>

namespace directdrop {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("DirectDrop — Wireless File Transfer"));
    setWindowIcon(QIcon(QStringLiteral("/usr/share/icons/hicolor/scalable/apps/directdrop.svg")));
    setMinimumSize(560, 480);
    resize(880, 680);
    setStyleSheet(QStringLiteral(
        "QWidget#centralGlass, QScrollArea, QStackedWidget, QWidget#pageSetup, QWidget#pageTransfer { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #060B1E, stop:0.5 #0D1B42, stop:1 #09132E); border: none; }"
        "QGroupBox { background: rgba(255, 255, 255, 0.10); border: 1.5px solid rgba(255, 255, 255, 0.28); border-radius: 20px; margin-top: 14px; padding: 14px; color: #E3F2FD; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 18px; padding: 0 8px; color: #82B1FF; font-weight: 700; }"
        "QLabel { color: #F0F6FF; background: transparent; }"
        "QPushButton { background: rgba(255, 255, 255, 0.14); border: 1.2px solid rgba(255, 255, 255, 0.38); border-radius: 19px; min-height: 36px; padding: 2px 20px; color: #FFFFFF; font-weight: 600; }"
        "QPushButton:hover { background: rgba(0, 229, 255, 0.32); border-color: rgba(255, 255, 255, 0.65); }"
        "QPushButton:pressed { background: rgba(41, 98, 255, 0.50); }"
        "QListWidget, QPlainTextEdit, QComboBox { background: rgba(255, 255, 255, 0.10); border: 1.2px solid rgba(255, 255, 255, 0.32); border-radius: 14px; color: #F0F6FF; padding: 6px 12px; }"
        "QComboBox QAbstractItemView { background: rgba(13, 27, 66, 0.88); border: 1.2px solid rgba(255, 255, 255, 0.38); border-radius: 12px; color: #FFFFFF; selection-background-color: rgba(0, 229, 255, 0.35); outline: none; }"
    ));

    m_p2p = new WifiP2pBackend(this);
    m_server = new TlsReceiverServer(this);

    m_pages = new QStackedWidget(this);

    // ================= PAGE 0: SETUP & DISCOVERY =================
    auto* scrollArea = new QScrollArea(m_pages);
    scrollArea->setWidgetResizable(true);
    auto* setupPage = new QWidget(scrollArea);
    setupPage->setObjectName(QStringLiteral("pageSetup"));
    auto* root = new QVBoxLayout(setupPage);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);

    auto* topBox = new QGroupBox(QStringLiteral("Connection & Wireless Band"), setupPage);
    auto* topLayout = new QHBoxLayout(topBox);
    m_lblTransport = new QLabel(QStringLiteral("Transport: Wi-Fi Direct (TLS 1.3)"), topBox);
    m_lblP2pStatus = new QLabel(QStringLiteral("Status: Ready"), topBox);
    m_bandCombo = new QComboBox(topBox);
    m_bandCombo->addItem(QStringLiteral("Band: Auto"));
    m_bandCombo->addItem(QStringLiteral("Band: 2.4 GHz"));
    m_bandCombo->addItem(QStringLiteral("Band: 5 GHz"));
    topLayout->addWidget(m_lblTransport);
    topLayout->addWidget(m_bandCombo);
    topLayout->addWidget(m_lblP2pStatus, 1);
    root->addWidget(topBox);

    auto* sendBox = new QGroupBox(QStringLiteral("Send Queue"), setupPage);
    auto* sendLayout = new QHBoxLayout(sendBox);
    m_lblSendQueue = new QLabel(QStringLiteral("No files queued"), sendBox);
    m_btnQueueFiles = new QPushButton(QStringLiteral("Select Files..."), sendBox);
    m_btnQueueFolder = new QPushButton(QStringLiteral("Select Folder..."), sendBox);
    m_btnSendQueued = new QPushButton(QStringLiteral("Send to Device"), sendBox);
    sendLayout->addWidget(m_lblSendQueue, 1);
    sendLayout->addWidget(m_btnQueueFiles);
    sendLayout->addWidget(m_btnQueueFolder);
    sendLayout->addWidget(m_btnSendQueued);
    root->addWidget(sendBox);

    auto* p2pBox = new QGroupBox(QStringLiteral("Nearby Devices"), setupPage);
    auto* p2pLayout = new QVBoxLayout(p2pBox);
    auto* p2pBtns = new QHBoxLayout();
    m_radarWidget = new RadarScanWidget(p2pBox);
    m_btnScan = new QPushButton(QStringLiteral("Discover Peers"), p2pBox);
    m_btnConnectPeer = new QPushButton(QStringLiteral("Connect Selected Peer"), p2pBox);
    p2pBtns->addWidget(m_radarWidget);
    p2pBtns->addWidget(m_btnScan);
    p2pBtns->addWidget(m_btnConnectPeer);
    p2pBtns->addStretch();
    m_peerList = new QListWidget(p2pBox);
    m_peerList->setMaximumHeight(110);
    p2pLayout->addLayout(p2pBtns);
    p2pLayout->addWidget(m_peerList);
    root->addWidget(p2pBox);

    auto* destBox = new QGroupBox(QStringLiteral("Receiving Folder & Policy"), setupPage);
    auto* destLayout = new QVBoxLayout(destBox);
    auto* destRow = new QHBoxLayout();
    m_lblDestPath = new QLabel(m_server->destinationRoot(), destBox);
    m_lblDestPath->setWordWrap(true);
    m_btnDest = new QPushButton(QStringLiteral("Choose Folder..."), destBox);
    m_btnOpenDest = new QPushButton(QStringLiteral("Open Folder"), destBox);
    destRow->addWidget(m_lblDestPath, 1);
    destRow->addWidget(m_btnDest);
    destRow->addWidget(m_btnOpenDest);
    auto* policyRow = new QHBoxLayout();
    m_lblDiskSpace = new QLabel(destBox);
    m_conflictCombo = new QComboBox(destBox);
    m_conflictCombo->addItem(QStringLiteral("Keep Both (Rename)"), static_cast<int>(ConflictPolicy::KeepBothRename));
    m_conflictCombo->addItem(QStringLiteral("Skip Existing"), static_cast<int>(ConflictPolicy::SkipExisting));
    m_conflictCombo->addItem(QStringLiteral("Replace (Confirm)"), static_cast<int>(ConflictPolicy::ReplaceWithConfirm));
    policyRow->addWidget(m_lblDiskSpace, 1);
    policyRow->addWidget(new QLabel(QStringLiteral("Conflicts:"), destBox));
    policyRow->addWidget(m_conflictCombo);
    destLayout->addLayout(destRow);
    destLayout->addLayout(policyRow);
    root->addWidget(destBox);

    auto* logBox = new QGroupBox(QStringLiteral("Activity Log"), setupPage);
    auto* logLayout = new QVBoxLayout(logBox);
    m_logView = new QPlainTextEdit(logBox);
    m_logView->setReadOnly(true);
    logLayout->addWidget(m_logView);
    root->addWidget(logBox, 1);

    scrollArea->setWidget(setupPage);
    m_pages->addWidget(scrollArea);

    // ================= PAGE 1: DEDICATED ACTIVE TRANSFER SCREEN =================
    auto* txPage = new QWidget(m_pages);
    txPage->setObjectName(QStringLiteral("pageTransfer"));
    auto* txRoot = new QVBoxLayout(txPage);
    txRoot->setContentsMargins(18, 18, 18, 18);
    txRoot->setSpacing(12);

    auto* txTopRow = new QHBoxLayout();
    m_btnBackToSetup = new QPushButton(QStringLiteral("← Back to Setup"), txPage);
    txTopRow->addWidget(m_btnBackToSetup);
    txTopRow->addStretch();
    txRoot->addLayout(txTopRow);

    auto* txBox = new QGroupBox(QStringLiteral("Active Transfer"), txPage);
    auto* txLayout = new QVBoxLayout(txBox);
    m_lblCurrentFile = new QLabel(QStringLiteral("Waiting for transfer..."), txBox);
    m_progressBar = new LiquidDropProgressBar(txBox);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    auto* statsRow = new QHBoxLayout();
    m_lblSpeed = new QLabel(QStringLiteral("Speed: 0.00 MB/s"), txBox);
    m_lblCounts = new QLabel(QStringLiteral("Verified: 0 | Failed: 0 | Skipped: 0"), txBox);
    statsRow->addWidget(m_lblSpeed);
    statsRow->addStretch();
    statsRow->addWidget(m_lblCounts);
    txLayout->addWidget(m_lblCurrentFile);
    txLayout->addWidget(m_progressBar);
    txLayout->addLayout(statsRow);
    txRoot->addWidget(txBox);

    auto* filesBox = new QGroupBox(QStringLiteral("Files in Transfer"), txPage);
    auto* filesLayout = new QVBoxLayout(filesBox);
    m_transferFileList = new QListWidget(filesBox);
    filesLayout->addWidget(m_transferFileList);
    txRoot->addWidget(filesBox, 1);

    m_pages->addWidget(txPage);
    setCentralWidget(m_pages);
    updateDiskSpaceLabel();

    connect(m_btnScan, &QPushButton::clicked, this, &MainWindow::onStartScan);
    connect(m_btnConnectPeer, &QPushButton::clicked, this, &MainWindow::onConnectSelectedPeer);
    connect(m_btnDest, &QPushButton::clicked, this, &MainWindow::onChooseDestination);
    connect(m_btnOpenDest, &QPushButton::clicked, this, &MainWindow::onOpenDestination);
    connect(m_btnQueueFiles, &QPushButton::clicked, this, &MainWindow::onQueueFilesToSend);
    connect(m_btnQueueFolder, &QPushButton::clicked, this, &MainWindow::onQueueFolderToSend);
    connect(m_btnSendQueued, &QPushButton::clicked, this, &MainWindow::onSendQueuedToPeer);
    connect(m_btnBackToSetup, &QPushButton::clicked, this, [this]() { m_pages->setCurrentIndex(0); });
    connect(m_conflictCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_server->setConflictPolicy(static_cast<ConflictPolicy>(m_conflictCombo->itemData(idx).toInt()));
    });

    connect(m_p2p, &WifiP2pBackend::peerAdded, this, &MainWindow::onPeerAdded);
    connect(m_p2p, &WifiP2pBackend::stateChanged, this, &MainWindow::onP2pStateChanged);
    connect(m_p2p, &WifiP2pBackend::p2pConnected, this, &MainWindow::onP2pConnected);
    connect(m_server, &TlsReceiverServer::logMessage, this, &MainWindow::appendLog);
    connect(m_server, &TlsReceiverServer::pairingRequested, this, &MainWindow::onPairingRequested);
    connect(m_server, &TlsReceiverServer::transferStarted, this, &MainWindow::onTransferStarted);
    connect(m_server, &TlsReceiverServer::fileProgress, this, &MainWindow::onFileProgress);
    connect(m_server, &TlsReceiverServer::transferProgress, this, &MainWindow::onTransferProgress);
    connect(m_server, &TlsReceiverServer::jobCompleted, this, &MainWindow::onJobCompleted);

    QString err;
    if (m_p2p->initialize(&err)) {
        appendLog(QStringLiteral("Wi-Fi P2P backend initialized: %1").arg(m_p2p->devicePath()));
    } else {
        appendLog(QStringLiteral("Wi-Fi P2P init warning: %1").arg(err));
    }

    QString listenErr;
    if (m_server->startListening(QHostAddress::AnyIPv4, 47291, &listenErr)) {
        m_receiverRunning = true;
    } else {
        appendLog(QStringLiteral("Receiver warning: %1").arg(listenErr));
    }
}

void MainWindow::onStartScan() {
    m_peerList->clear();
    QString err;
    if (!m_p2p->startDiscovery(&err)) {
        appendLog(QStringLiteral("Discovery error: %1").arg(err));
    }
}

void MainWindow::onConnectSelectedPeer() {
    auto* item = m_peerList->currentItem();
    if (!item) return;
    QString err;
    if (!m_p2p->connectToPeer(item->data(Qt::UserRole).toString(), &err)) {
        appendLog(QStringLiteral("Connect error: %1").arg(err));
    }
}

void MainWindow::onChooseDestination() {
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Receiving Folder"), m_server->destinationRoot());
    if (!dir.isEmpty()) {
        m_server->setDestinationRoot(dir);
        m_lblDestPath->setText(dir);
        updateDiskSpaceLabel();
    }
}

void MainWindow::onOpenDestination() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_server->destinationRoot()));
}

void MainWindow::onQueueFilesToSend() {
    QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Select Files to Queue"));
    if (files.isEmpty()) return;
    m_queuedSendFiles = files;
    m_queuedSendBaseDir.clear();
    m_lblSendQueue->setText(QStringLiteral("Queued: %1 file(s) ready to send").arg(files.size()));
}

void MainWindow::onQueueFolderToSend() {
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Folder to Queue"));
    if (dir.isEmpty()) return;
    QStringList files;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) files.append(it.next());
    if (files.isEmpty()) return;
    m_queuedSendFiles = files;
    m_queuedSendBaseDir = dir;
    m_lblSendQueue->setText(QStringLiteral("Queued: %1 file(s) from folder ready").arg(files.size()));
}

void MainWindow::onSendQueuedToPeer() {
    if (m_queuedSendFiles.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Nothing Selected"), QStringLiteral("Select files or a folder first."));
        return;
    }
    QString host = m_p2p->gatewayIp();
    if (host.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Not Connected"), QStringLiteral("Connect to a Wi-Fi Direct peer first."));
        return;
    }
    TlsSenderClient client;
    QString err;
    if (client.sendFiles(host, 47291, m_queuedSendFiles, m_queuedSendBaseDir, &err)) {
        appendLog(QStringLiteral("Sent %1 queued file(s) to %2").arg(m_queuedSendFiles.size()).arg(host));
    } else {
        QMessageBox::warning(this, QStringLiteral("Send Failed"), err);
    }
}

void MainWindow::onPeerAdded(const P2pPeerInfo& peer) {
    auto* item = new QListWidgetItem(QStringLiteral("%1 (%2) — %3%").arg(peer.name, peer.hwAddress).arg(peer.strength), m_peerList);
    item->setData(Qt::UserRole, peer.objectPath);
}

void MainWindow::onP2pStateChanged(P2pState state, const QString& detail) {
    if (auto* radar = qobject_cast<RadarScanWidget*>(m_radarWidget)) {
        radar->setScanning(state == P2pState::Searching);
    }
    m_lblP2pStatus->setText(QStringLiteral("Status: %1").arg(detail));
    appendLog(QStringLiteral("[P2P] %1").arg(detail));
}

void MainWindow::onP2pConnected(const QString& iface, const QString& localIp, const QString& gatewayIp) {
    appendLog(QStringLiteral("P2P link ready on %1: local=%2 gateway=%3").arg(iface, localIp, gatewayIp));
}

void MainWindow::onPairingRequested(const QString& deviceName, const QString& deviceId, const QString& sasCode) {
    auto res = QMessageBox::question(
        this,
        QStringLiteral("Confirm Pairing"),
        QStringLiteral("Device '%1' (%2) wants to connect.\n\nVerification Code: %3\n\nDoes this match the code on your phone?")
            .arg(deviceName, deviceId, sasCode),
        QMessageBox::Yes | QMessageBox::No
    );
    m_server->respondToPairing(deviceId, res == QMessageBox::Yes);
}

void MainWindow::onTransferStarted(int totalFiles, uint64_t) {
    m_transferFileList->clear();
    m_fileItems.clear();
    m_progressBar->setValue(0);
    m_lblCurrentFile->setText(QStringLiteral("Starting transfer (%1 files)...").arg(totalFiles));
    m_pages->setCurrentIndex(1); // Switch to Dedicated Transfer Screen
}

void MainWindow::onFileProgress(const QString& relPath, uint64_t receivedBytes, uint64_t totalBytes, bool verified) {
    double rxMb = receivedBytes / (1024.0 * 1024.0);
    double totMb = totalBytes / (1024.0 * 1024.0);
    QString status = verified ? QStringLiteral("✓ Verified") : QStringLiteral("Receiving...");
    QString label = QStringLiteral("%1 — %2 / %3 MB (%4)")
                        .arg(relPath)
                        .arg(rxMb, 0, 'f', 2)
                        .arg(totMb, 0, 'f', 2)
                        .arg(status);

    if (!m_fileItems.contains(relPath)) {
        auto* item = new QListWidgetItem(label, m_transferFileList);
        m_fileItems.insert(relPath, item);
        m_transferFileList->scrollToItem(item);
    } else {
        m_fileItems[relPath]->setText(label);
    }
}

void MainWindow::onTransferProgress(const TransferStats& stats) {
    m_lblCurrentFile->setText(QStringLiteral("Receiving: %1").arg(stats.currentFile));
    int pct = stats.totalBytes > 0 ? static_cast<int>((stats.receivedBytes * 100) / stats.totalBytes) : 0;
    m_progressBar->setValue(pct);
    m_lblSpeed->setText(QStringLiteral("Speed: %1 MB/s (avg %2 MB/s)")
                            .arg(stats.currentMbps, 0, 'f', 2)
                            .arg(stats.averageMbps, 0, 'f', 2));
    m_lblCounts->setText(QStringLiteral("Verified: %1/%2 | Failed: %3 | Skipped: %4")
                             .arg(stats.verifiedFiles).arg(stats.totalFiles)
                             .arg(stats.failedFiles).arg(stats.skippedFiles));
}

void MainWindow::onJobCompleted(const TransferStats& stats) {
    m_progressBar->setValue(100);
    m_lblCurrentFile->setText(QStringLiteral("Transfer Complete!"));
    appendLog(QStringLiteral("Job completed: %1/%2 verified, %3 failed, %4 skipped, avg %5 MB/s")
                  .arg(stats.verifiedFiles).arg(stats.totalFiles)
                  .arg(stats.failedFiles).arg(stats.skippedFiles)
                  .arg(stats.averageMbps, 0, 'f', 2));
    updateDiskSpaceLabel();
}

void MainWindow::appendLog(const QString& msg) {
    m_logView->appendPlainText(msg);
}

void MainWindow::updateDiskSpaceLabel() {
    QStorageInfo info(m_server->destinationRoot());
    if (info.isValid()) {
        double gb = info.bytesAvailable() / (1024.0 * 1024.0 * 1024.0);
        m_lblDiskSpace->setText(QStringLiteral("Available space: %1 GB").arg(gb, 0, 'f', 2));
    } else {
        m_lblDiskSpace->setText(QStringLiteral("Available space: Unknown"));
    }
}

} // namespace directdrop
