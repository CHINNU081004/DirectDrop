#pragma once

#include <QProcess>
#include <QString>

namespace directdrop {

enum class BandMode {
    AutoRecommended = 0,
    Band24GHzRange = 1,
    Band5GHzSpeed = 2
};

struct AdapterCapabilities {
    bool checked = false;
    bool supports5GHz = false;
    bool supportsP2p = false;
    bool supportsConcurrent = false;
    QString summary;
};

class AdaptiveWifiPolicy {
public:
    static AdapterCapabilities probeCapabilities() {
        AdapterCapabilities cap;
        cap.checked = true;
        QProcess proc;
        proc.start(QStringLiteral("iw"), {QStringLiteral("list")});
        if (!proc.waitForFinished(3000) || proc.exitCode() != 0) {
            cap.summary = QStringLiteral("Capability probe unavailable (iw not found); using conservative Auto.");
            return cap;
        }
        QString out = QString::fromUtf8(proc.readAllStandardOutput());
        cap.supports5GHz = out.contains(QStringLiteral("5180 MHz")) || out.contains(QStringLiteral("5200 MHz"));
        cap.supportsP2p = out.contains(QStringLiteral("P2P-client")) && out.contains(QStringLiteral("P2P-GO"));
        cap.supportsConcurrent = out.contains(QStringLiteral("#channels <= 2"));
        cap.summary = QStringLiteral("P2P=%1 • 5 GHz=%2 • Concurrent=%3")
            .arg(cap.supportsP2p ? QStringLiteral("Yes") : QStringLiteral("No"))
            .arg(cap.supports5GHz ? QStringLiteral("Yes") : QStringLiteral("No"))
            .arg(cap.supportsConcurrent ? QStringLiteral("Yes") : QStringLiteral("Single-channel"));
        return cap;
    }

    static QString describeMode(BandMode mode, const AdapterCapabilities& cap) {
        switch (mode) {
            case BandMode::AutoRecommended:
                return cap.supports5GHz
                    ? QStringLiteral("Auto (Recommended): Prefers 5 GHz when peer supports it; 2.4 GHz fallback for range.")
                    : QStringLiteral("Auto (Recommended): 2.4 GHz (5 GHz not reported by local adapter).");
            case BandMode::Band24GHzRange:
                return QStringLiteral("2.4 GHz (Range): Prioritizes wall penetration & long-range stability.");
            case BandMode::Band5GHzSpeed:
                return cap.supports5GHz
                    ? QStringLiteral("5 GHz (Speed): Requests 5 GHz for high throughput (channel negotiated by wpa_supplicant).")
                    : QStringLiteral("5 GHz (Speed) requested, but local adapter did not report 5 GHz; fallback to Auto/2.4 GHz.");
        }
        return {};
    }
};

} // namespace directdrop
