#pragma once

#include "AdaptiveWifiPolicy.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace directdrop {

/**
 * Dedicated in-app Learn More page explaining Wi-Fi bands, hardware vs Wi-Fi Direct
 * vs Router/LAN capabilities, peer compatibility, router impact, and device-specific gaps.
 */
class LearnMorePage : public QWidget {
    Q_OBJECT
public:
    explicit LearnMorePage(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("pageLearnMore"));
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(20, 20, 20, 20);
        root->setSpacing(12);

        auto* header = new QHBoxLayout();
        auto* btnBack = new QPushButton(QStringLiteral("← Back to Connect Device"), this);
        btnBack->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        auto* title = new QLabel(QStringLiteral("Understanding Wi-Fi Bands & Transfer Capabilities"), this);
        title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: #FFFFFF;"));
        header->addWidget(btnBack);
        header->addWidget(title, 1);
        root->addLayout(header);

        connect(btnBack, &QPushButton::clicked, this, &LearnMorePage::backRequested);

        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        auto* body = new QWidget(scroll);
        body->setObjectName(QStringLiteral("pageOverview"));
        auto* bodyLayout = new QVBoxLayout(body);
        bodyLayout->setSpacing(12);

        auto addSection = [&](const QString& heading, QLabel** targetLabel, const QString& initialText) {
            auto* box = new QGroupBox(heading, body);
            auto* bl = new QVBoxLayout(box);
            auto* lbl = new QLabel(initialText, box);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(QStringLiteral("color: rgba(241,245,249,0.92); font-size: 13px; line-height: 1.45;"));
            bl->addWidget(lbl);
            bodyLayout->addWidget(box);
            if (targetLabel) *targetLabel = lbl;
        };

        addSection(
            QStringLiteral("1. Understanding Wi-Fi Bands"),
            &m_secBands,
            QStringLiteral(
                "• 2.4 GHz: Usually offers better range and wall penetration, often with more interference and congestion. Practical for older devices and distant rooms.\n"
                "• 5 GHz: Often provides higher potential throughput and more channel capacity than congested 2.4 GHz, with shorter range through walls.\n"
                "• 6 GHz: Used by compatible Wi-Fi 6E and Wi-Fi 7 hardware for extra channel capacity. Shorter range through obstacles; actual availability depends on hardware, OS, driver, router, and local regulations.\n"
                "Note: No frequency band guarantees a fixed transfer speed."));

        addSection(
            QStringLiteral("2. Why 6 GHz Wi-Fi Might Work While Wi-Fi Direct Does Not"),
            &m_secMismatch,
            QString());

        addSection(
            QStringLiteral("3. Why the Other Device Matters"),
            &m_secPeer,
            QStringLiteral(
                "A transfer depends on both endpoints and the chosen transport. If one device does not support 6 GHz, both devices cannot connect directly on 6 GHz.\n"
                "However, over a Wi-Fi router (LAN), a dual-band or tri-band router can connect each device on a different band (e.g., 6 GHz on one device, 5 GHz on the other) and bridge local traffic cleanly."));

        addSection(
            QStringLiteral("4. Why a Powerful Router Can Improve Transfer Speeds"),
            &m_secRouter,
            QStringLiteral(
                "Capable routers can improve real throughput via better radios/antennas, wider channels, lower congestion, multi-stream MIMO, and steadier coverage.\n"
                "However, a router alone does not set your speed: the slower endpoint, signal strength, link rate, storage write speed, and CPU also limit throughput. A 6 GHz router is not automatically faster than every 5 GHz router and cannot add 6 GHz support to incompatible devices."));

        addSection(
            QStringLiteral("5. What Is Missing on This Device?"),
            &m_secMissing,
            QString());

        bodyLayout->addStretch();
        scroll->setWidget(body);
        root->addWidget(scroll, 1);
    }

    void updateFromReport(const AdapterCapabilities& local, const TransferCompatibilityReport& report) {
        if (local.wifi.band6 == CapabilityState::Supported && local.wifiDirect.band6 != CapabilityState::Supported) {
            m_secMismatch->setText(QStringLiteral(
                "This device supports 6 GHz Wi-Fi hardware, but its current OS or Wi-Fi Direct stack does not expose a usable 6 GHz Wi-Fi Direct mode.\n"
                "You may still transfer over a compatible 6 GHz router/LAN if both devices share the same local network.\n"
                "Layers: Wi-Fi HW [2.4G=%1, 5G=%2, 6G=%3] • Wi-Fi Direct=%4 • Active Link=%5")
                .arg(AdaptiveWifiPolicy::stateText(local.wifi.band24),
                     AdaptiveWifiPolicy::stateText(local.wifi.band5),
                     AdaptiveWifiPolicy::stateText(local.wifi.band6),
                     AdaptiveWifiPolicy::stateText(local.wifiDirect.available),
                     report.activeBandLabel));
        } else {
            m_secMismatch->setText(QStringLiteral(
                "Wi-Fi hardware support, OS driver exposure, Wi-Fi Direct group-band control, Router/LAN reachability, and the active link frequency are independent layers.\n"
                "Current status: Wi-Fi HW [2.4G=%1, 5G=%2, 6G=%3] • Wi-Fi Direct=%4 • Active Link=%5")
                .arg(AdaptiveWifiPolicy::stateText(local.wifi.band24),
                     AdaptiveWifiPolicy::stateText(local.wifi.band5),
                     AdaptiveWifiPolicy::stateText(local.wifi.band6),
                     AdaptiveWifiPolicy::stateText(local.wifiDirect.available),
                     report.activeBandLabel));
        }

        if (report.missingDiagnostics.isEmpty()) {
            m_secMissing->setText(QStringLiteral("No capability gaps detected for the current configuration (%1).").arg(report.recommendationHeadline));
        } else {
            m_secMissing->setText(QStringLiteral("• ") + report.missingDiagnostics.join(QStringLiteral("\n• ")));
        }
    }

signals:
    void backRequested();

private:
    QLabel* m_secBands = nullptr;
    QLabel* m_secMismatch = nullptr;
    QLabel* m_secPeer = nullptr;
    QLabel* m_secRouter = nullptr;
    QLabel* m_secMissing = nullptr;
};

} // namespace directdrop
