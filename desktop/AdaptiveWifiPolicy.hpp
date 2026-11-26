#pragma once

#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QSysInfo>

namespace directdrop {

enum class BandMode {
    AutoRecommended = 0,
    Band24GHz = 1,
    Band5GHz = 2,
    Band6GHz = 3
};

enum class CapabilityState {
    Supported,
    Unsupported,
    PermissionRequired,
    Unknown
};

enum class TransportKind {
    WifiDirectP2p,
    RouterLan
};

enum class ReachabilityState {
    NotTested,
    Reachable,
    Unreachable,
    ClientIsolationSuspected
};

struct WifiBandCapabilities {
    CapabilityState band24 = CapabilityState::Unknown;
    CapabilityState band5 = CapabilityState::Unknown;
    CapabilityState band6 = CapabilityState::Unknown;
};

struct WifiDirectCapabilities {
    CapabilityState available = CapabilityState::Unknown;
    CapabilityState band24 = CapabilityState::Unknown;
    CapabilityState band5 = CapabilityState::Unknown;
    CapabilityState band6 = CapabilityState::Unsupported; // NM wifi-p2p does not expose 6 GHz P2P forcing
    bool bandControlExposed = false;
};

struct RouterLanCapabilities {
    CapabilityState available = CapabilityState::Unknown;
    int activeFrequencyMhz = 0;
    double linkRateMbps = 0.0; // >0 only when reported by driver/iw
    QString activeInterface;
    QStringList lanAddresses;
};

struct AdapterCapabilities {
    bool checked = false;
    bool iwAvailable = false;
    QString osName;
    QString cpuArch;
    WifiBandCapabilities wifi;
    WifiDirectCapabilities wifiDirect;
    RouterLanCapabilities routerLan;
    QString summary;

    // Compatibility accessors kept for existing callers/tests
    bool supports24GHz() const { return wifi.band24 == CapabilityState::Supported; }
    bool supports5GHz() const { return wifi.band5 == CapabilityState::Supported; }
    bool supports6GHz() const { return wifi.band6 == CapabilityState::Supported; }
    bool supportsP2p() const { return wifiDirect.available == CapabilityState::Supported; }
};

struct PeerCapabilities {
    QString name = QStringLiteral("Peer Device");
    bool known = false;
    WifiBandCapabilities wifi;
    CapabilityState wifiDirectAvailable = CapabilityState::Unknown;
    CapabilityState routerLanAvailable = CapabilityState::Unknown;
    int activeFrequencyMhz = 0;
};

struct ComparisonRow {
    QString label;
    QString localValue;
    QString peerValue;
};

struct TransferCompatibilityReport {
    TransportKind recommendedTransport = TransportKind::RouterLan;
    BandMode recommendedBand = BandMode::AutoRecommended;
    QString recommendationHeadline;
    QString recommendationReason;
    QString activeBandLabel;
    QString expectedPerformance;
    QStringList missingDiagnostics;
    QList<ComparisonRow> rows;
};

class AdaptiveWifiPolicy {
public:
    static QString stateText(CapabilityState s) {
        switch (s) {
            case CapabilityState::Supported: return QStringLiteral("Supported");
            case CapabilityState::Unsupported: return QStringLiteral("Unsupported");
            case CapabilityState::PermissionRequired: return QStringLiteral("Permission required");
            case CapabilityState::Unknown: return QStringLiteral("Unknown");
        }
        return QStringLiteral("Unknown");
    }

    static QString bandLabel(BandMode m) {
        switch (m) {
            case BandMode::AutoRecommended: return QStringLiteral("Auto");
            case BandMode::Band24GHz: return QStringLiteral("2.4 GHz");
            case BandMode::Band5GHz: return QStringLiteral("5 GHz");
            case BandMode::Band6GHz: return QStringLiteral("6 GHz");
        }
        return QStringLiteral("Auto");
    }

    static QString activeBandFromMhz(int mhz) {
        if (mhz >= 2400 && mhz <= 2500) return QStringLiteral("2.4 GHz (%1 MHz)").arg(mhz);
        if (mhz >= 5150 && mhz <= 5900) return QStringLiteral("5 GHz (%1 MHz)").arg(mhz);
        if (mhz >= 5925 && mhz <= 7125) return QStringLiteral("6 GHz (%1 MHz)").arg(mhz);
        return QStringLiteral("Unknown / not reported");
    }

    static AdapterCapabilities parseIwOutput(const QString& iwListOut, const QString& iwLinkOut = QString(), bool iwPresent = true) {
        AdapterCapabilities cap;
        cap.checked = true;
        cap.iwAvailable = iwPresent && !iwListOut.isEmpty();
        cap.osName = QSysInfo::prettyProductName();
        cap.cpuArch = QSysInfo::currentCpuArchitecture();

        if (cap.iwAvailable) {
            static const QRegularExpression freqRe(QStringLiteral(R"(\b(\d{4})(?:\.\d+)?\s*MHz\b)"));
            auto it = freqRe.globalMatch(iwListOut);
            bool seen24 = false, seen5 = false, seen6 = false;
            while (it.hasNext()) {
                int mhz = it.next().captured(1).toInt();
                if (mhz >= 2400 && mhz <= 2500) seen24 = true;
                else if (mhz >= 5150 && mhz <= 5900) seen5 = true;
                else if (mhz >= 5925 && mhz <= 7125) seen6 = true;
            }
            cap.wifi.band24 = seen24 ? CapabilityState::Supported : CapabilityState::Unsupported;
            cap.wifi.band5 = seen5 ? CapabilityState::Supported : CapabilityState::Unsupported;
            cap.wifi.band6 = seen6 ? CapabilityState::Supported : CapabilityState::Unsupported;

            bool p2p = iwListOut.contains(QStringLiteral("P2P-client")) &&
                       iwListOut.contains(QStringLiteral("P2P-GO"));
            cap.wifiDirect.available = p2p ? CapabilityState::Supported : CapabilityState::Unsupported;
            cap.wifiDirect.band24 = p2p ? cap.wifi.band24 : CapabilityState::Unsupported;
            cap.wifiDirect.band5 = p2p ? cap.wifi.band5 : CapabilityState::Unsupported;
            cap.wifiDirect.band6 = CapabilityState::Unsupported; // NM wifi-p2p delegates channel to wpa_supplicant
            cap.wifiDirect.bandControlExposed = false;
        } else {
            // Missing `iw` must report Unknown rather than Unsupported
            cap.wifi.band24 = CapabilityState::Unknown;
            cap.wifi.band5 = CapabilityState::Unknown;
            cap.wifi.band6 = CapabilityState::Unknown;
            cap.wifiDirect.available = CapabilityState::Unknown;
        }

        static const QRegularExpression linkFreqRe(QStringLiteral(R"(freq:\s*(\d{4}))"));
        auto lm = linkFreqRe.match(iwLinkOut);
        if (lm.hasMatch()) cap.routerLan.activeFrequencyMhz = lm.captured(1).toInt();

        static const QRegularExpression rateRe(QStringLiteral(R"(tx bitrate:\s*([0-9]+(?:\.[0-9]+)?)\s*MBit/s)"));
        auto rm = rateRe.match(iwLinkOut);
        if (rm.hasMatch()) cap.routerLan.linkRateMbps = rm.captured(1).toDouble();

        cap.summary = QStringLiteral("Wi-Fi HW [2.4G=%1, 5G=%2, 6G=%3] • P2P=%4 • Arch=%5")
            .arg(stateText(cap.wifi.band24), stateText(cap.wifi.band5), stateText(cap.wifi.band6),
                 stateText(cap.wifiDirect.available), cap.cpuArch);
        return cap;
    }

    static AdapterCapabilities probeCapabilities() {
        QProcess proc;
        proc.start(QStringLiteral("iw"), {QStringLiteral("list")});
        bool iwOk = proc.waitForFinished(2500) && proc.exitCode() == 0;
        QString iwOut = iwOk ? QString::fromUtf8(proc.readAllStandardOutput()) : QString();

        QString linkOut;
        QString activeIface;
        QStringList lanIps;
        for (const auto& iface : QNetworkInterface::allInterfaces()) {
            if (!(iface.flags() & QNetworkInterface::IsUp) ||
                (iface.flags() & QNetworkInterface::IsLoopBack)) {
                continue;
            }
            for (const auto& entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    lanIps.append(entry.ip().toString());
                    if (activeIface.isEmpty() && iface.name().startsWith(QStringLiteral("wl"))) {
                        activeIface = iface.name();
                    }
                }
            }
        }
        if (!activeIface.isEmpty() && iwOk) {
            QProcess linkProc;
            linkProc.start(QStringLiteral("iw"), {QStringLiteral("dev"), activeIface, QStringLiteral("link")});
            if (linkProc.waitForFinished(1500) && linkProc.exitCode() == 0) {
                linkOut = QString::fromUtf8(linkProc.readAllStandardOutput());
            }
        }

        AdapterCapabilities cap = parseIwOutput(iwOut, linkOut, iwOk);
        cap.routerLan.activeInterface = activeIface;
        cap.routerLan.lanAddresses = lanIps;
        cap.routerLan.available = lanIps.isEmpty() ? CapabilityState::Unsupported : CapabilityState::Supported;
        return cap;
    }

    static QString estimatePerformance(const AdapterCapabilities& local, TransportKind transport) {
        if (transport == TransportKind::RouterLan && local.routerLan.linkRateMbps > 0.0) {
            double lowMBs = qMax(1.0, (local.routerLan.linkRateMbps * 0.40) / 8.0);
            double highMBs = qMax(lowMBs, (local.routerLan.linkRateMbps * 0.65) / 8.0);
            return QStringLiteral("Link rate: %1 Mbps • Estimated TCP range: ~%2–%3 MB/s (depends on router, signal, & storage)")
                .arg(local.routerLan.linkRateMbps, 0, 'f', 0)
                .arg(lowMBs, 0, 'f', 1)
                .arg(highMBs, 0, 'f', 1);
        }
        return QStringLiteral("Estimated throughput unknown until link rate is reported or transfer starts measuring.");
    }

    static TransferCompatibilityReport compareAndRecommend(
        const AdapterCapabilities& local,
        const PeerCapabilities& peer,
        TransportKind selectedTransport,
        BandMode requestedBand,
        ReachabilityState lanReachability
    ) {
        TransferCompatibilityReport r;
        r.activeBandLabel = local.routerLan.activeFrequencyMhz > 0
            ? activeBandFromMhz(local.routerLan.activeFrequencyMhz)
            : QStringLiteral("Unknown / not reported");
        r.expectedPerformance = estimatePerformance(local, selectedTransport);

        auto peerState = [&](CapabilityState s) { return peer.known ? stateText(s) : QStringLiteral("Unknown (not queried)"); };
        r.rows = {
            {QStringLiteral("2.4 GHz Wi-Fi HW/OS"), stateText(local.wifi.band24), peerState(peer.wifi.band24)},
            {QStringLiteral("5 GHz Wi-Fi HW/OS"), stateText(local.wifi.band5), peerState(peer.wifi.band5)},
            {QStringLiteral("6 GHz Wi-Fi HW/OS"), stateText(local.wifi.band6), peerState(peer.wifi.band6)},
            {QStringLiteral("Wi-Fi Direct"), stateText(local.wifiDirect.available), peerState(peer.wifiDirectAvailable)},
            {QStringLiteral("Router/LAN Path"), stateText(local.routerLan.available),
             lanReachability == ReachabilityState::Reachable ? QStringLiteral("Reachable") :
             lanReachability == ReachabilityState::ClientIsolationSuspected ? QStringLiteral("Blocked / isolation suspected") :
             lanReachability == ReachabilityState::Unreachable ? QStringLiteral("Unreachable") : peerState(peer.routerLanAvailable)},
            {QStringLiteral("Active Network Freq"), r.activeBandLabel,
             peer.activeFrequencyMhz > 0 ? activeBandFromMhz(peer.activeFrequencyMhz) : QStringLiteral("Unknown")}
        };

        // Missing / limitation diagnostics
        if (!local.iwAvailable) {
            r.missingDiagnostics << QStringLiteral("`iw` command unavailable or restricted; hardware band capabilities are Unknown.");
        }
        if (local.wifi.band6 == CapabilityState::Supported && local.wifiDirect.band6 != CapabilityState::Supported) {
            r.missingDiagnostics << QStringLiteral("Your device supports 6 GHz Wi-Fi, but 6 GHz Wi-Fi Direct is not exposed by this platform.");
        } else if (local.wifi.band6 == CapabilityState::Unsupported) {
            r.missingDiagnostics << QStringLiteral("Your device does not report 6 GHz Wi-Fi support.");
        }
        if (peer.known && peer.wifi.band6 != CapabilityState::Supported && requestedBand == BandMode::Band6GHz) {
            r.missingDiagnostics << QStringLiteral("The other device does not confirm 6 GHz support; a dual/tri-band router may still bridge endpoints over LAN.");
        }
        if (lanReachability == ReachabilityState::ClientIsolationSuspected || lanReachability == ReachabilityState::Unreachable) {
            r.missingDiagnostics << QStringLiteral("LAN peer is unreachable (check firewall, subnet, or router AP client isolation).");
        } else if (local.routerLan.available == CapabilityState::Supported && lanReachability == ReachabilityState::NotTested) {
            r.missingDiagnostics << QStringLiteral("Both devices have LAN interfaces, but DirectDrop has not verified local reachability yet.");
        }

        // Recommendation logic
        if (lanReachability == ReachabilityState::Reachable) {
            r.recommendedTransport = TransportKind::RouterLan;
            r.recommendedBand = requestedBand;
            r.recommendationHeadline = QStringLiteral("Recommended: Router/LAN (TLS 1.3) — peer reachable");
            if (requestedBand != BandMode::AutoRecommended && local.routerLan.activeFrequencyMhz > 0) {
                r.recommendationReason = QStringLiteral("Requested %1 through router; active OS link reports %2. Dual/tri-band routers can bridge different bands across endpoints.")
                    .arg(bandLabel(requestedBand), r.activeBandLabel);
            } else {
                r.recommendationReason = QStringLiteral("Local network path verified; preserves TLS 1.3, SAS pairing, and SHA-256 verification.");
            }
        } else if (selectedTransport == TransportKind::WifiDirectP2p && local.wifiDirect.available == CapabilityState::Supported) {
            r.recommendedTransport = TransportKind::WifiDirectP2p;
            if (requestedBand == BandMode::Band6GHz) {
                r.recommendedBand = BandMode::AutoRecommended;
                r.recommendationHeadline = QStringLiteral("6 GHz Wi-Fi Direct unavailable — use Auto/5 GHz P2P or Router/LAN");
                r.recommendationReason = QStringLiteral("Wi-Fi Direct does not expose 6 GHz here. Switch to Router/LAN on a 6 GHz AP or choose Auto/5 GHz.");
            } else {
                r.recommendedBand = requestedBand;
                r.recommendationHeadline = QStringLiteral("Recommended: Wi-Fi Direct (%1)").arg(bandLabel(requestedBand));
                r.recommendationReason = QStringLiteral("Direct peer-to-peer link without requiring a shared router.");
            }
        } else if (local.routerLan.available == CapabilityState::Supported) {
            r.recommendedTransport = TransportKind::RouterLan;
            r.recommendedBand = requestedBand;
            r.recommendationHeadline = QStringLiteral("Recommended: Through Wi-Fi Router (LAN)");
            r.recommendationReason = QStringLiteral("Wi-Fi Direct is unavailable or not selected; test LAN reachability or enter peer IP.");
        } else {
            r.recommendedTransport = TransportKind::RouterLan;
            r.recommendedBand = BandMode::AutoRecommended;
            r.recommendationHeadline = QStringLiteral("Auto mode — connect Wi-Fi or verify permissions");
            r.recommendationReason = QStringLiteral("Neither Wi-Fi Direct nor an active LAN IP was confirmed.");
        }
        return r;
    }
};

} // namespace directdrop
