#include "MainWindow.hpp"
#include "AdaptiveWifiPolicy.hpp"
#include "FlowLayout.hpp"
#include "LiquidBandSelector.hpp"
#include "LiquidDeviceCard.hpp"
#include "LiquidDropProgressBar.hpp"
#include "LiquidSidebar.hpp"
#include "RadarScanWidget.hpp"
#include "TlsClient.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QStorageInfo>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace directdrop {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("DirectDrop — Wireless File Transfer"));
    setWindowIcon(QIcon(QStringLiteral("/usr/share/icons/hicolor/scalable/apps/directdrop.svg")));
    setMinimumSize(780, 560);
    resize(1040, 680);
    setStyleSheet(QStringLiteral(
        "QWidget#shellRoot, QScrollArea, QStackedWidget, QWidget#pageOverview, QWidget#pageFiles, QWidget#pageConnect, QWidget#pageTransfer, QWidget#pageStorage { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0B1220, stop:0.5 #141E30, stop:1 #0F172A); border: none; }"
        "QWidget#liquidSidebar { background: rgba(255, 255, 255, 0.04); border-right: 1px solid rgba(255, 255, 255, 0.16); }"
        "QGroupBox { background: rgba(255, 255, 255, 0.07); border: 1.2px solid rgba(255, 255, 255, 0.22); border-radius: 18px; margin-top: 14px; padding: 14px; color: #F1F5F9; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 16px; padding: 0 8px; color: rgba(255,255,255,0.88); font-weight: 700; }"
        "QLabel { color: #F1F5F9; background: transparent; }"
        "QPushButton { background: rgba(255, 255, 255, 0.10); border: 1.2px solid rgba(255, 255, 255, 0.28); border-radius: 17px; min-height: 34px; padding: 4px 14px; color: #F8FAFC; font-size: 13px; font-weight: 600; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.20); border-color: rgba(255, 255, 255, 0.52); }"
        "QPushButton:pressed { background: rgba(255, 255, 255, 0.26); }"
        "QPushButton:checked { background: rgba(255, 255, 255, 0.22); border-color: rgba(255, 255, 255, 0.68); color: #FFFFFF; }"
        "QPushButton:disabled { background: rgba(255, 255, 255, 0.04); border-color: rgba(255, 255, 255, 0.12); color: rgba(255, 255, 255, 0.40); }"
        "QListWidget, QPlainTextEdit, QComboBox, QLineEdit { background: rgba(255, 255, 255, 0.07); border: 1.2px solid rgba(255, 255, 255, 0.24); border-radius: 12px; color: #F1F5F9; padding: 6px 12px; font-size: 13px; }"
        "QListWidget::item:selected { background: rgba(255, 255, 255, 0.16); border-radius: 8px; color: #FFFFFF; }"
        "QListWidget::item:hover { background: rgba(255, 255, 255, 0.09); border-radius: 8px; }"
        "QComboBox QAbstractItemView { background: rgba(15, 23, 42, 0.94); border: 1.2px solid rgba(255, 255, 255, 0.34); border-radius: 12px; color: #FFFFFF; selection-background-color: rgba(255, 255, 255, 0.20); outline: none; }"
        "QDialog, QMessageBox { background: #0F172A; color: #F1F5F9; }"
        "QToolTip { background: rgba(15, 23, 42, 0.94); color: #F8FAFC; border: 1px solid rgba(255, 255, 255, 0.32); border-radius: 8px; padding: 5px 8px; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: rgba(255, 255, 255, 0.22); border-radius: 4px; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    ));

    m_p2p = new WifiP2pBackend(this);
    m_server = new TlsReceiverServer(this);

    auto* shell = new QWidget(this);
    shell->setObjectName(QStringLiteral("shellRoot"));
    auto* shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto* sidebar = new LiquidSidebar(shell);
    m_sidebar = sidebar;
    shellLayout->addWidget(sidebar);

    m_pages = new QStackedWidget(shell);
    shellLayout->addWidget(m_pages, 1);

    // ================= PAGE 0: OVERVIEW =================
    auto* overviewScroll = new QScrollArea(m_pages);
    overviewScroll->setWidgetResizable(true);
    auto* overviewPage = new QWidget(overviewScroll);
    overviewPage->setObjectName(QStringLiteral("pageOverview"));
    auto* ovLayout = new QVBoxLayout(overviewPage);
    ovLayout->setContentsMargins(20, 20, 20, 20);
    ovLayout->setSpacing(14);

    auto* topBox = new QGroupBox(QStringLiteral("Connection && Adaptive Wi-Fi Policy"), overviewPage);
    auto* topVLayout = new QVBoxLayout(topBox);
    auto* topHost = new QWidget(topBox);
    auto* topLayout = new FlowLayout(topHost, 0, 12, 8);
    m_lblTransport = new QLabel(QStringLiteral("Transport: Wi-Fi Direct (TLS 1.3)"), topHost);
    m_lblP2pStatus = new QLabel(QStringLiteral("Status: Ready"), topHost);
    auto* bandSel = new LiquidBandSelector(topHost);
    m_bandSelector = bandSel;
    m_btnOpenConnectPage = new QPushButton(QStringLiteral("Connect Device →"), topHost);
    m_btnOpenConnectPage->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    topLayout->addWidget(m_lblTransport);
    topLayout->addWidget(bandSel);
    topLayout->addWidget(m_lblP2pStatus);
    topLayout->addWidget(m_btnOpenConnectPage);
    auto cap = AdaptiveWifiPolicy::probeCapabilities();
    m_lblAdapterCap = new QLabel(QStringLiteral("Adapter: %1").arg(cap.summary), topBox);
    m_lblAdapterCap->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.72); font-size: 12px;"));
    m_lblBandPolicy = new QLabel(AdaptiveWifiPolicy::describeMode(BandMode::AutoRecommended, cap), topBox);
    m_lblBandPolicy->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.82); font-size: 12px;"));
    m_lblBandPolicy->setWordWrap(true);
    topVLayout->addWidget(topHost);
    topVLayout->addWidget(m_lblAdapterCap);
    topVLayout->addWidget(m_lblBandPolicy);
    ovLayout->addWidget(topBox);

    connect(bandSel, &LiquidBandSelector::bandChanged, this, [this, cap](int idx, const QString&) {
        m_lblBandPolicy->setText(AdaptiveWifiPolicy::describeMode(static_cast<BandMode>(idx), cap));
    });

    auto* logBox = new QGroupBox(QStringLiteral("Activity Log"), overviewPage);
    auto* logLayout = new QVBoxLayout(logBox);
    m_logView = new QPlainTextEdit(logBox);
    m_logView->setReadOnly(true);
    logLayout->addWidget(m_logView);
    ovLayout->addWidget(logBox, 1);
    overviewScroll->setWidget(overviewPage);
    m_pages->addWidget(overviewScroll);

    // ================= PAGE 1: NATIVE FILE MANAGER =================
    auto* filesPage = new QWidget(m_pages);
    filesPage->setObjectName(QStringLiteral("pageFiles"));
    auto* filesLayout = new QVBoxLayout(filesPage);
    filesLayout->setContentsMargins(20, 20, 20, 20);
    filesLayout->setSpacing(10);

    auto* fmBox = new QGroupBox(QStringLiteral("File Manager (Images • Files • Folders)"), filesPage);
    auto* fmBoxLayout = new QVBoxLayout(fmBox);
    m_fileManager = new FileManagerWidget(fmBox);
    fmBoxLayout->addWidget(m_fileManager);
    filesLayout->addWidget(fmBox, 1);
    m_pages->addWidget(filesPage);

    // ================= PAGE 2: CONNECT DEVICE =================
    auto* connectPage = new QWidget(m_pages);
    connectPage->setObjectName(QStringLiteral("pageConnect"));
    auto* connRoot = new QVBoxLayout(connectPage);
    connRoot->setContentsMargins(20, 20, 20, 20);
    connRoot->setSpacing(12);

    auto* connHeaderHost = new QWidget(connectPage);
    auto* connHeader = new FlowLayout(connHeaderHost, 0, 10, 8);
    m_btnBackFromConnect = new QPushButton(QStringLiteral("← Overview"), connHeaderHost);
    auto* lblConnTitle = new QLabel(QStringLiteral("Connect Device"), connHeaderHost);
    lblConnTitle->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: #FFFFFF; padding: 4px 6px;"));
    m_radarWidget = new RadarScanWidget(connHeaderHost);
    m_btnScan = new QPushButton(QStringLiteral("Rescan"), connHeaderHost);
    m_btnConnectPeer = new QPushButton(QStringLiteral("Connect Selected"), connHeaderHost);
    for (auto* b : {m_btnBackFromConnect, m_btnScan, m_btnConnectPeer}) {
        b->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }
    connHeader->addWidget(m_btnBackFromConnect);
    connHeader->addWidget(lblConnTitle);
    connHeader->addWidget(m_radarWidget);
    connHeader->addWidget(m_btnScan);
    connHeader->addWidget(m_btnConnectPeer);
    connRoot->addWidget(connHeaderHost);

    auto* connBox = new QGroupBox(QStringLiteral("Nearby Devices"), connectPage);
    auto* connBoxLayout = new QVBoxLayout(connBox);
    m_deviceCardsHost = new QWidget(connBox);
    auto* cardsLayout = new QVBoxLayout(m_deviceCardsHost);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setSpacing(8);
    cardsLayout->addStretch();
    m_peerList = new QListWidget(connBox);
    m_peerList->setMaximumHeight(90);
    connBoxLayout->addWidget(m_deviceCardsHost, 1);
    connBoxLayout->addWidget(m_peerList);
    connRoot->addWidget(connBox, 1);
    m_pages->addWidget(connectPage);

    // ================= PAGE 3: TRANSFERS =================
    auto* txPage = new QWidget(m_pages);
    txPage->setObjectName(QStringLiteral("pageTransfer"));
    auto* txRoot = new QVBoxLayout(txPage);
    txRoot->setContentsMargins(20, 20, 20, 20);
    txRoot->setSpacing(12);

    auto* txTopRow = new QHBoxLayout();
    m_btnBackToSetup = new QPushButton(QStringLiteral("← Overview"), txPage);
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
    auto* txFilesLayout = new QVBoxLayout(filesBox);
    m_transferFileList = new QListWidget(filesBox);
    txFilesLayout->addWidget(m_transferFileList);
    txRoot->addWidget(filesBox, 1);
    m_pages->addWidget(txPage);

    // ================= PAGE 4: STORAGE =================
    auto* storagePage = new QWidget(m_pages);
    storagePage->setObjectName(QStringLiteral("pageStorage"));
    auto* stLayout = new QVBoxLayout(storagePage);
    stLayout->setContentsMargins(20, 20, 20, 20);

    auto* destBox = new QGroupBox(QStringLiteral("Receiving Folder & Conflict Policy"), storagePage);
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
    stLayout->addWidget(destBox);
    stLayout->addStretch();
    m_pages->addWidget(storagePage);

    setCentralWidget(shell);
    updateDiskSpaceLabel();

    connect(sidebar, &LiquidSidebar::navigationSelected, this, &MainWindow::setPageIndex);
    connect(m_btnScan, &QPushButton::clicked, this, &MainWindow::onStartScan);
    connect(m_btnConnectPeer, &QPushButton::clicked, this, &MainWindow::onConnectSelectedPeer);
    connect(m_btnDest, &QPushButton::clicked, this, &MainWindow::onChooseDestination);
    connect(m_btnOpenDest, &QPushButton::clicked, this, &MainWindow::onOpenDestination);
    connect(m_fileManager, &FileManagerWidget::sendRequested, this, [this](const QStringList& files, const QString& baseDir) {
        m_queuedSendFiles = files;
        m_queuedSendBaseDir = baseDir;
        onSendQueuedToPeer();
    });
    connect(m_btnOpenConnectPage, &QPushButton::clicked, this, [this]() {
        setPageIndex(2);
        onStartScan();
    });
    connect(m_btnBackFromConnect, &QPushButton::clicked, this, [this]() { setPageIndex(0); });
    connect(m_btnBackToSetup, &QPushButton::clicked, this, [this]() { setPageIndex(0); });
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

void MainWindow::setPageIndex(int idx) {
    if (m_pages) m_pages->setCurrentIndex(idx);
    if (auto* sb = qobject_cast<LiquidSidebar*>(m_sidebar)) {
        sb->setActiveIndex(idx);
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

void MainWindow::onQueueFilesToSend() {}
void MainWindow::onQueueFolderToSend() {}

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

    if (m_deviceCardsHost && m_deviceCardsHost->layout()) {
        auto* card = new LiquidDeviceCard(peer.name, peer.hwAddress, peer.strength, peer.objectPath, m_deviceCardsHost);
        connect(card, &LiquidDeviceCard::selectedWithOrigin, this, [this](const QString& path, const QPointF&) {
            QString err;
            if (!m_p2p->connectToPeer(path, &err)) {
                appendLog(QStringLiteral("Connect error: %1").arg(err));
            }
        });
        auto* vbox = qobject_cast<QVBoxLayout*>(m_deviceCardsHost->layout());
        if (vbox) vbox->insertWidget(qMax(0, vbox->count() - 1), card);
    }
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
    setPageIndex(3);
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
