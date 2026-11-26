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
#include <QHostInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QSettings>
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
    m_lanDiscovery = new LanDiscoveryService(this);
    m_adapterCap = AdaptiveWifiPolicy::probeCapabilities();
    if (!m_adapterCap.supportsP2p() && m_adapterCap.routerLan.available == CapabilityState::Supported) {
        m_activeTransport = TransportKind::RouterLan;
    }

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
    auto cap = m_adapterCap;
    m_lblAdapterCap = new QLabel(QStringLiteral("Adapter: %1").arg(cap.summary), topBox);
    m_lblAdapterCap->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.72); font-size: 12px;"));
    m_lblBandPolicy = new QLabel(
        AdaptiveWifiPolicy::compareAndRecommend(cap, m_peerCap, m_activeTransport, BandMode::AutoRecommended, m_lanReachability).recommendationReason,
        topBox);
    m_lblBandPolicy->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.82); font-size: 12px;"));
    m_lblBandPolicy->setWordWrap(true);
    topVLayout->addWidget(topHost);
    topVLayout->addWidget(m_lblAdapterCap);
    topVLayout->addWidget(m_lblBandPolicy);
    ovLayout->addWidget(topBox);

    connect(bandSel, &LiquidBandSelector::bandChanged, this, [this](int idx, const QString&) {
        applyBandPreference(idx, true);
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
    auto* connectScroll = new QScrollArea(m_pages);
    connectScroll->setWidgetResizable(true);
    auto* connectPage = new QWidget(connectScroll);
    connectPage->setObjectName(QStringLiteral("pageConnect"));
    auto* connRoot = new QVBoxLayout(connectPage);
    connRoot->setContentsMargins(20, 20, 20, 20);
    connRoot->setSpacing(10);

    auto* connHeaderHost = new QWidget(connectPage);
    auto* connHeader = new FlowLayout(connHeaderHost, 0, 10, 8);
    m_btnBackFromConnect = new QPushButton(QStringLiteral("← Overview"), connHeaderHost);
    auto* lblConnTitle = new QLabel(QStringLiteral("Connect Device"), connHeaderHost);
    lblConnTitle->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: #FFFFFF; padding: 4px 6px;"));
    m_radarWidget = new RadarScanWidget(connHeaderHost);
    m_btnScan = new QPushButton(QStringLiteral("Rescan"), connHeaderHost);
    m_btnConnectPeer = new QPushButton(QStringLiteral("Connect Selected"), connHeaderHost);
    m_btnLearnMore = new QPushButton(QStringLiteral("Learn more →"), connHeaderHost);
    for (auto* b : {m_btnBackFromConnect, m_btnScan, m_btnConnectPeer, m_btnLearnMore}) {
        b->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }
    connHeader->addWidget(m_btnBackFromConnect);
    connHeader->addWidget(lblConnTitle);
    connHeader->addWidget(m_radarWidget);
    connHeader->addWidget(m_btnScan);
    connHeader->addWidget(m_btnConnectPeer);
    connHeader->addWidget(m_btnLearnMore);
    connRoot->addWidget(connHeaderHost);

    auto* bandBox = new QGroupBox(QStringLiteral("Transport && Wi-Fi Band Preference"), connectPage);
    auto* bandBoxLayout = new QVBoxLayout(bandBox);
    bandBoxLayout->setSpacing(8);
    auto* transportSel = new LiquidSegmentedSelector(
        {QStringLiteral("Direct (Wi-Fi Direct)"), QStringLiteral("Through Wi-Fi Router (LAN)")}, bandBox);
    m_transportSelector = transportSel;
    transportSel->setCurrentIndex(m_activeTransport == TransportKind::WifiDirectP2p ? 0 : 1, false);
    auto* connBandSel = new LiquidBandSelector(bandBox);
    m_connectBandSelector = connBandSel;
    m_lblConnBandStatus = new QLabel(bandBox);
    m_lblConnBandStatus->setStyleSheet(QStringLiteral("font-weight: 700; color: #FFFFFF; font-size: 13px;"));
    m_lblConnBandStatus->setWordWrap(true);
    m_lblConnWhy = new QLabel(bandBox);
    m_lblConnWhy->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.80); font-size: 12px;"));
    m_lblConnWhy->setWordWrap(true);
    m_lblExpectedPerf = new QLabel(bandBox);
    m_lblExpectedPerf->setStyleSheet(QStringLiteral("color: rgba(226,232,240,0.78); font-size: 12px;"));
    m_lblExpectedPerf->setWordWrap(true);
    bandBoxLayout->addWidget(transportSel);
    bandBoxLayout->addWidget(connBandSel);
    bandBoxLayout->addWidget(m_lblConnBandStatus);
    bandBoxLayout->addWidget(m_lblConnWhy);
    bandBoxLayout->addWidget(m_lblExpectedPerf);
    connRoot->addWidget(bandBox);

    connect(transportSel, &LiquidSegmentedSelector::selectionChanged, this, [this](int idx, const QString&) {
        m_activeTransport = (idx == 0) ? TransportKind::WifiDirectP2p : TransportKind::RouterLan;
        refreshCompatibilityUi();
    });
    connect(connBandSel, &LiquidBandSelector::bandChanged, this, [this](int idx, const QString&) {
        applyBandPreference(idx, true);
    });

    auto* compatBox = new QGroupBox(QStringLiteral("Transfer Compatibility (This Device vs Peer)"), connectPage);
    auto* compatLayout = new QVBoxLayout(compatBox);
    m_lblCompatTable = new QLabel(compatBox);
    m_lblCompatTable->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 12px; color: rgba(241,245,249,0.92);"));
    m_lblCompatTable->setWordWrap(true);
    compatLayout->addWidget(m_lblCompatTable);
    connRoot->addWidget(compatBox);

    auto* connBox = new QGroupBox(QStringLiteral("Nearby Devices (Wi-Fi Direct)"), connectPage);
    auto* connBoxLayout = new QVBoxLayout(connBox);
    m_deviceCardsHost = new QWidget(connBox);
    auto* cardsLayout = new QVBoxLayout(m_deviceCardsHost);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setSpacing(8);
    cardsLayout->addStretch();
    m_peerList = new QListWidget(connBox);
    m_peerList->setMaximumHeight(80);
    connBoxLayout->addWidget(m_deviceCardsHost, 1);
    connBoxLayout->addWidget(m_peerList);
    connRoot->addWidget(connBox);

    auto* lanBox = new QGroupBox(QStringLiteral("Through Wi-Fi Router (Secure LAN • TLS 1.3)"), connectPage);
    auto* lanLayout = new QVBoxLayout(lanBox);
    QString localIps = m_adapterCap.routerLan.lanAddresses.isEmpty()
        ? QStringLiteral("No active LAN IP")
        : m_adapterCap.routerLan.lanAddresses.join(QStringLiteral(", "));
    auto* lblLocalLan = new QLabel(QStringLiteral("This device LAN endpoint: %1 : 47291").arg(localIps), lanBox);
    lblLocalLan->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.80); font-size: 12px;"));
    m_lanPeerList = new QListWidget(lanBox);
    m_lanPeerList->setMaximumHeight(72);
    auto* lanRow = new QHBoxLayout();
    m_lanIpEdit = new QLineEdit(lanBox);
    m_lanIpEdit->setPlaceholderText(QStringLiteral("Manual peer IP (e.g. 192.168.1.42 or 192.168.1.42:47291)"));
    m_btnTestLan = new QPushButton(QStringLiteral("Verify LAN"), lanBox);
    m_btnConnectLan = new QPushButton(QStringLiteral("Use Router/LAN"), lanBox);
    m_btnTestLan->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_btnConnectLan->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    lanRow->addWidget(m_lanIpEdit, 1);
    lanRow->addWidget(m_btnTestLan);
    lanRow->addWidget(m_btnConnectLan);
    lanLayout->addWidget(lblLocalLan);
    lanLayout->addWidget(m_lanPeerList);
    lanLayout->addLayout(lanRow);
    connRoot->addWidget(lanBox);

    connectScroll->setWidget(connectPage);
    m_pages->addWidget(connectScroll);

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
        m_lanDiscovery->start(QHostInfo::localHostName(), 47291, m_adapterCap);
    } else {
        appendLog(QStringLiteral("Receiver warning: %1").arg(listenErr));
    }

    m_learnMorePage = new LearnMorePage(m_pages);
    m_pages->addWidget(m_learnMorePage);
    connect(m_btnLearnMore, &QPushButton::clicked, this, [this]() {
        refreshCompatibilityUi();
        m_pages->setCurrentWidget(m_learnMorePage);
    });
    connect(m_learnMorePage, &LearnMorePage::backRequested, this, [this]() {
        setPageIndex(2);
    });

    connect(m_lanDiscovery, &LanDiscoveryService::peersUpdated, this, [this](const QList<LanPeerInfo>& peers) {
        m_lanPeerList->clear();
        for (const auto& p : peers) {
            auto* it = new QListWidgetItem(
                QStringLiteral("%1 (%2:%3) — %4").arg(p.deviceName, p.ip).arg(p.tlsPort).arg(p.bands),
                m_lanPeerList);
            it->setData(Qt::UserRole, p.ip);
            it->setData(Qt::UserRole + 1, static_cast<int>(p.tlsPort));
            if (m_lanTargetIp.isEmpty() || m_lanTargetIp == p.ip) {
                m_peerCap = p.caps;
            }
        }
        refreshCompatibilityUi();
    });
    connect(m_lanPeerList, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        if (!it) return;
        const QString ip = it->data(Qt::UserRole).toString();
        const int port = it->data(Qt::UserRole + 1).toInt();
        m_lanIpEdit->setText(QStringLiteral("%1:%2").arg(ip).arg(port));
        for (const auto& p : m_lanDiscovery->peers()) {
            if (p.ip == ip) {
                m_peerCap = p.caps;
                refreshCompatibilityUi();
                break;
            }
        }
    });
    auto parseLanInput = [this](QString* outIp, quint16* outPort) -> bool {
        const QString raw = m_lanIpEdit->text().trimmed();
        if (raw.isEmpty()) return false;
        const QStringList parts = raw.split(':');
        QHostAddress addr(parts[0]);
        if (addr.isNull()) return false;
        *outIp = parts[0];
        *outPort = (parts.size() > 1 && parts[1].toUShort() > 0) ? parts[1].toUShort() : 47291;
        return true;
    };
    connect(m_btnTestLan, &QPushButton::clicked, this, [this, parseLanInput]() {
        QString ip;
        quint16 port = 47291;
        if (!parseLanInput(&ip, &port)) {
            QMessageBox::information(this, QStringLiteral("Enter Valid IP"), QStringLiteral("Enter a valid peer LAN IP first (e.g. 192.168.1.42)."));
            return;
        }
        m_lanReachability = LanDiscoveryService::probeTcpReachability(ip, port);
        refreshCompatibilityUi();
    });
    connect(m_btnConnectLan, &QPushButton::clicked, this, [this, parseLanInput]() {
        QString ip;
        quint16 port = 47291;
        if (!parseLanInput(&ip, &port)) {
            QMessageBox::warning(this, QStringLiteral("Invalid IP"), QStringLiteral("Enter a valid peer LAN IP (e.g. 192.168.1.42) or tap a discovered LAN peer."));
            return;
        }
        m_lanTargetIp = ip;
        m_lanTargetPort = port;
        m_activeTransport = TransportKind::RouterLan;
        if (auto* ts = qobject_cast<LiquidSegmentedSelector*>(m_transportSelector)) {
            ts->setCurrentIndex(1, false);
        }
        m_lanReachability = LanDiscoveryService::probeTcpReachability(ip, port);
        m_lblP2pStatus->setText(QStringLiteral("Transport: Router/LAN (%1:%2)").arg(m_lanTargetIp).arg(m_lanTargetPort));
        appendLog(QStringLiteral("Router/LAN target set to %1:%2 (TLS 1.3 + SAS)").arg(m_lanTargetIp).arg(m_lanTargetPort));
        refreshCompatibilityUi();
    });

    QSettings settings(QStringLiteral("DirectDrop"), QStringLiteral("DirectDrop"));
    int savedBand = qBound(0, settings.value(QStringLiteral("bandPreference"), 0).toInt(), 3);
    applyBandPreference(savedBand, false);
}

void MainWindow::applyBandPreference(int idx, bool persist) {
    int clamped = qBound(0, idx, 3);
    if (auto* ovBand = qobject_cast<LiquidBandSelector*>(m_bandSelector)) {
        if (ovBand->currentIndex() != clamped) ovBand->setCurrentIndex(clamped, false);
    }
    if (auto* connBand = qobject_cast<LiquidBandSelector*>(m_connectBandSelector)) {
        if (connBand->currentIndex() != clamped) connBand->setCurrentIndex(clamped, false);
    }
    if (persist) {
        QSettings settings(QStringLiteral("DirectDrop"), QStringLiteral("DirectDrop"));
        settings.setValue(QStringLiteral("bandPreference"), clamped);
    }
    refreshCompatibilityUi();
}

void MainWindow::refreshCompatibilityUi() {
    int bandIdx = 0;
    if (auto* ovBand = qobject_cast<LiquidBandSelector*>(m_bandSelector)) {
        bandIdx = ovBand->currentIndex();
    }
    auto report = AdaptiveWifiPolicy::compareAndRecommend(
        m_adapterCap,
        m_peerCap,
        m_activeTransport,
        static_cast<BandMode>(bandIdx),
        m_lanReachability);

    if (m_lblBandPolicy) m_lblBandPolicy->setText(report.recommendationReason);
    if (m_lblConnBandStatus) {
        m_lblConnBandStatus->setText(QStringLiteral("%1 • Active Link: %2").arg(report.recommendationHeadline, report.activeBandLabel));
    }
    if (m_lblConnWhy) {
        m_lblConnWhy->setText(QStringLiteral("Why this option? %1").arg(report.recommendationReason));
    }
    if (m_lblExpectedPerf) {
        m_lblExpectedPerf->setText(QStringLiteral("Expected performance: %1").arg(report.expectedPerformance));
    }
    if (m_lblCompatTable) {
        QStringList lines;
        for (const auto& row : report.rows) {
            lines << QStringLiteral("%1 | You: %2 | Peer: %3")
                         .arg(row.label.leftJustified(20), row.localValue.leftJustified(14), row.peerValue);
        }
        m_lblCompatTable->setText(lines.join(QChar('\n')));
    }
    if (m_learnMorePage) {
        m_learnMorePage->updateFromReport(m_adapterCap, report);
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
    quint16 port = 47291;
    if (host.isEmpty() && !m_lanTargetIp.isEmpty()) {
        host = m_lanTargetIp;
        port = m_lanTargetPort;
    }
    if (host.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Not Connected"), QStringLiteral("Connect to a Wi-Fi Direct peer or set a Secure LAN target first."));
        return;
    }
    TlsSenderClient client;
    QString err;
    if (client.sendFiles(host, port, m_queuedSendFiles, m_queuedSendBaseDir, &err)) {
        appendLog(QStringLiteral("Sent %1 queued file(s) to %2:%3").arg(m_queuedSendFiles.size()).arg(host).arg(port));
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
