#pragma once

#include "LiquidGlassPhysics.hpp"
#include <QPainter>
#include <QProgressBar>
#include <QTimer>
#include <QtMath>

namespace directdrop {

class LiquidDropProgressBar : public QProgressBar {
    Q_OBJECT
public:
    explicit LiquidDropProgressBar(QWidget* parent = nullptr) : QProgressBar(parent) {
        setMinimumHeight(32);
        setTextVisible(false);
        setValue(0);

        m_animTimer = new QTimer(this);
        m_animTimer->setInterval(16); // ~60fps fluid interpolation
        connect(m_animTimer, &QTimer::timeout, this, [this]() {
            int val = qMax(0, value());
            double target = (maximum() > minimum())
                ? static_cast<double>(val - minimum()) / (maximum() - minimum())
                : 0.0;
            target = qBound(0.0, target, 1.0);
            m_phase += 0.14;
            double diff = target - m_visualFrac;
            if (qAbs(diff) < 0.002) {
                m_visualFrac = target;
            } else {
                m_visualFrac += diff * 0.18;
            }
            update();
        });
        m_animTimer->start();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRectF r = rect().adjusted(4, 6, -4, -6);
        double frac = qBound(0.0, m_visualFrac, 1.0);

        // Frosted glass track
        p.setBrush(QColor(255, 255, 255, 34));
        p.setPen(QPen(QColor(255, 255, 255, 90), 1.2));
        p.drawRoundedRect(r, r.height() / 2.0, r.height() / 2.0);

        // Restrained transparent glass fill stream
        if (frac > 0.001) {
            double fillW = qMax(r.height(), r.width() * frac);
            QRectF fillRect(r.left(), r.top(), fillW, r.height());
            QLinearGradient grad(fillRect.topLeft(), fillRect.topRight());
            grad.setColorAt(0.0, QColor(255, 255, 255, 28));
            grad.setColorAt(1.0, QColor(255, 255, 255, 72));
            p.setBrush(grad);
            p.setPen(QPen(QColor(255, 255, 255, 120), 1.1));
            p.drawRoundedRect(fillRect, r.height() / 2.0, r.height() / 2.0);
        }

        // Transparent water droplet lens at leading edge
        double dropX = r.left() + (r.width() - r.height()) * frac + r.height() / 2.0;
        double dropY = r.center().y();
        double wobble = 1.0 + 0.05 * qSin(m_phase);
        double dropRx = r.height() * 0.72 * wobble;
        double dropRy = r.height() * 0.72 / wobble;
        QRectF dropRect(dropX - dropRx, dropY - dropRy * 0.88, dropRx * 2.0, dropRy * 1.76);
        drawTransparentGlassLens(p, dropRect, dropRy * 0.88, QPointF(dropX - dropRx * 0.25, dropY - dropRy * 0.3), 1.0);

        // Percentage label
        p.setPen(QColor(255, 255, 255));
        p.setFont(QFont(font().family(), 9, QFont::Bold));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("%1%").arg(qRound(frac * 100.0)));
    }

private:
    QTimer* m_animTimer = nullptr;
    double m_visualFrac = 0.0;
    double m_phase = 0.0;
};

} // namespace directdrop
