#pragma once

#include <QPainter>
#include <QTimer>
#include <QWidget>

namespace directdrop {

class RadarScanWidget : public QWidget {
    Q_OBJECT
public:
    explicit RadarScanWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(56, 56);
        m_timer = new QTimer(this);
        m_timer->setInterval(16);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            if (!m_scanning) return;
            m_phase += 0.025;
            if (m_phase >= 1.0) m_phase -= 1.0;
            update();
        });
    }

    void setScanning(bool scanning) {
        m_scanning = scanning;
        if (scanning && !m_timer->isActive()) m_timer->start();
        if (!scanning) m_timer->stop();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPointF c(width() / 2.0, height() / 2.0);
        double maxR = width() / 2.0 - 4.0;

        // Center droplet dot
        p.setBrush(m_scanning ? QColor(0, 229, 255) : QColor(130, 177, 255, 160));
        p.setPen(Qt::NoPen);
        p.drawEllipse(c, 5.0, 5.0);

        if (m_scanning) {
            for (int i = 0; i < 3; ++i) {
                double t = m_phase + i / 3.0;
                if (t >= 1.0) t -= 1.0;
                double r = 6.0 + (maxR - 6.0) * t;
                int alpha = static_cast<int>(210 * (1.0 - t));
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(0, 229, 255, alpha), 2.0));
                p.drawEllipse(c, r, r);
            }
        } else {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 255, 255, 65), 1.2));
            p.drawEllipse(c, maxR * 0.65, maxR * 0.65);
        }
    }

private:
    QTimer* m_timer = nullptr;
    bool m_scanning = false;
    double m_phase = 0.0;
};

} // namespace directdrop
