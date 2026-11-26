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
    SupportedAndAvailable,
    SupportedButUnavailable,
    Unsupported,
    PermissionRequired,
    Unknown
};

enum class TransportKind {
    WifiDirectP2p,
    SecureLanTcp,
    ManualIpTls
};

struct AdapterCapabilities {
    bool checked = false;
    QString osName;
    QString cpuArch;
    bool supports24GHz = false;
    bool supports5GHz = false;
    bool supports6GHz = false;
    bool supportsP2p = false;
    bool supportsConcurrent = false;
    bool p2pBandControlExposed = false; // Linux NM wifi-p2p delegates channel to wpa_supplicant
    bool hasLanInterface = false;
    int activeFrequencyMhz = 0;         // >0 only when reliably queried from kernel/iw
    QString activeInterface;
    QStringList lanAddresses;
    QString summary;
};

struct PeerCapabilities {
    bool supports24GHz = true;
    bool supports5GHz = false;
    bool supports6GHz = false;
    bool supportsP2p = false;
    bool supportsLan = true;
};

struct BandDecision {
    BandMode requestedMode = BandMode::AutoRecommended;
    BandMode effectiveTarget = BandMode::Band24GHz;
    CapabilityState requestedState = CapabilityState::Unknown;
    bool canHonorDirectly = false;
    bool fallbackRecommended = false;
    QString statusHeadline;
    QString explanation;
    QString activeBandLabel;
};

class AdaptiveWifiPolicy {
public:
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
        return QStringLiteral("Band information unavailable");
    }

    static AdapterCapabilities parseIwOutput(const QString& iwListOut, const QString& iwLinkOut = QString()) {
        AdapterCapabilities cap;
        cap.checked = true;
        cap.osName = QSysInfo::prettyProductName();
        cap.cpuArch = QSysInfo::currentCpuArchitecture();

        // Parse MHz entries reliably: e.g. "2412 MHz", "5180 MHz", "5955 MHz"
        static const QRegularExpression freqRe(QStringLiteral(R"(\b(\d{4})\s*MHz\b)"));
        auto it = freqRe.globalMatch(iwListOut);
        while (it.hasNext()) {
            int mhz = it.next().captured(1).toInt();
            if (mhz >= 2400 && mhz <= 2500) cap.supports24GHz = true;
            else if (mhz >= 5150 && mhz <= 5900) cap.supports5GHz = true;
            else if (mhz >= 5925 && mhz <= 7125) cap.supports6GHz = true;
        }

        cap.supportsP2p = iwListOut.contains(QStringLiteral("P2P-client")) &&
                          iwListOut.contains(QStringLiteral("P2P-GO"));
        cap.supportsConcurrent = iwListOut.contains(QStringLiteral("#channels <= 2"));
        cap.p2pBandControlExposed = false; // NM wifi-p2p D-Bus profile does not expose fixed band forcing

        static const QRegularExpression linkFreqRe(QStringLiteral(R"(freq:\s*(\d{4}))"));
        auto lm = linkFreqRe.match(iwLinkOut);
        if (lm.hasMatch()) {
            cap.activeFrequencyMhz = lm.captured(1).toInt();
        }

        cap.summary = QStringLiteral("2.4G=%1 • 5G=%2 • 6G=%3 • P2P=%4 • Arch=%5")
            .arg(cap.supports24GHz ? QStringLiteral("Yes") : QStringLiteral("No"))
            .arg(cap.supports5GHz ? QStringLiteral("Yes") : QStringLiteral("No"))
            .arg(cap.supports6GHz ? QStringLiteral("Yes") : QStringLiteral("No"))
            .arg(cap.supportsP2p ? QStringLiteral("Yes") : QStringLiteral("No"))
            .arg(cap.cpuArch);
        return cap;
    }

    static AdapterCapabilities probeCapabilities() {
        QProcess proc;
        proc.start(QStringLiteral("iw"), {QStringLiteral("list")});
        QString iwOut;
        if (proc.waitForFinished(2500) && proc.exitCode() == 0) {
            iwOut = QString::fromUtf8(proc.readAllStandardOutput());
        }

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
        if (!activeIface.isEmpty()) {
            QProcess linkProc;
            linkProc.start(QStringLiteral("iw"), {QStringLiteral("dev"), activeIface, QStringLiteral("link")});
            if (linkProc.waitForFinished(1500) && linkProc.exitCode() == 0) {
                linkOut = QString::fromUtf8(linkProc.readAllStandardOutput());
            }
        }

        AdapterCapabilities cap = parseIwOutput(iwOut, linkOut);
        if (iwOut.isEmpty()) {
            cap.summary = QStringLiteral("iw unavailable on %1 (%2); LAN fallback ready")
                              .arg(cap.osName, cap.cpuArch);
        }
        cap.activeInterface = activeIface;
        cap.lanAddresses = lanIps;
        cap.hasLanInterface = !lanIps.isEmpty();
        return cap;
    }

    static BandDecision evaluateBand(
        BandMode requested,
        const AdapterCapabilities& local,
        const PeerCapabilities& peer,
        TransportKind transport
    ) {
        BandDecision d;
        d.requestedMode = requested;
        d.activeBandLabel = local.activeFrequencyMhz > 0
            ? activeBandFromMhz(local.activeFrequencyMhz)
            : QStringLiteral("Band information unavailable");

        if (requested == BandMode::AutoRecommended) {
            d.requestedState = CapabilityState::SupportedAndAvailable;
            d.canHonorDirectly = true;
            if (local.supports6GHz && peer.supports6GHz && transport != TransportKind::WifiDirectP2p) {
                d.effectiveTarget = BandMode::Band6GHz;
            } else if (local.supports5GHz && peer.supports5GHz) {
                d.effectiveTarget = BandMode::Band5GHz;
            } else {
                d.effectiveTarget = BandMode::Band24GHz;
            }
            d.statusHeadline = QStringLiteral("Auto — choosing the best available connection");
            d.explanation = (transport == TransportKind::WifiDirectP2p && !local.p2pBandControlExposed)
                ? QStringLiteral("Auto lets wpa_supplicant negotiate the best mutual channel (%1).")
                      .arg(local.supports5GHz ? QStringLiteral("5 GHz preferred, 2.4 GHz fallback") : QStringLiteral("2.4 GHz"))
                : QStringLiteral("Secure LAN uses the active OS/router Wi-Fi band (%1).").arg(d.activeBandLabel);
            return d;
        }

        auto checkBand = [&](bool localSup, bool peerSup, const QString& label) {
            if (!localSup) {
                d.requestedState = CapabilityState::Unsupported;
                d.canHonorDirectly = false;
                d.fallbackRecommended = true;
                d.statusHeadline = QStringLiteral("%1 unavailable — choose Auto or another band").arg(label);
                d.explanation = QStringLiteral("Local adapter did not report %1 support (preference preserved).").arg(label);
                return;
            }
            if (!peerSup) {
                d.requestedState = CapabilityState::SupportedButUnavailable;
                d.canHonorDirectly = false;
                d.fallbackRecommended = true;
                d.statusHeadline = QStringLiteral("%1 supported, but unavailable for this connection").arg(label);
                d.explanation = QStringLiteral("Peer does not advertise %1 compatibility; choose Auto or a mutually supported band.").arg(label);
                return;
            }
            if (transport == TransportKind::WifiDirectP2p && !local.p2pBandControlExposed) {
                d.requestedState = CapabilityState::SupportedButUnavailable;
                d.canHonorDirectly = false;
                d.fallbackRecommended = true;
                d.statusHeadline = QStringLiteral("%1 supported, but unavailable for Wi-Fi Direct control").arg(label);
                d.explanation = QStringLiteral("Linux NetworkManager Wi-Fi Direct delegates channel selection to wpa_supplicant. Use Auto or Secure LAN on a %1 AP.").arg(label);
                return;
            }
            d.requestedState = CapabilityState::SupportedAndAvailable;
            d.canHonorDirectly = true;
            d.effectiveTarget = requested;
            d.statusHeadline = QStringLiteral("Manual preference: %1").arg(label);
            d.explanation = (transport == TransportKind::SecureLanTcp || transport == TransportKind::ManualIpTls)
                ? QStringLiteral("DirectDrop cannot force a router's band; active OS link reports: %1.").arg(d.activeBandLabel)
                : QStringLiteral("Requesting %1 on supported transport.").arg(label);
        };

        if (requested == BandMode::Band24GHz) checkBand(local.supports24GHz, peer.supports24GHz, QStringLiteral("2.4 GHz"));
        else if (requested == BandMode::Band5GHz) checkBand(local.supports5GHz, peer.supports5GHz, QStringLiteral("5 GHz"));
        else if (requested == BandMode::Band6GHz) checkBand(local.supports6GHz, peer.supports6GHz, QStringLiteral("6 GHz"));
        return d;
    }

    static QString describeMode(BandMode mode, const AdapterCapabilities& cap) {
        return evaluateBand(mode, cap, PeerCapabilities{true, cap.supports5GHz, cap.supports6GHz, cap.supportsP2p, true},
                            cap.supportsP2p ? TransportKind::WifiDirectP2p : TransportKind::SecureLanTcp).explanation;
    }
};

} // namespace directdrop
