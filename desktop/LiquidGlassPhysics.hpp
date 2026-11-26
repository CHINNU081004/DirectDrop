#pragma once

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRadialGradient>
#include <QtMath>

namespace directdrop {

struct Spring1D {
    double value = 0.0;
    double velocity = 0.0;
    double target = 0.0;
    double stiffness = 260.0;
    double damping = 22.0;

    bool step(double dt) {
        double force = -stiffness * (value - target) - damping * velocity;
        velocity += force * dt;
        value += velocity * dt;
        return qAbs(value - target) > 0.001 || qAbs(velocity) > 0.001;
    }
};

struct LiquidRipple {
    bool active = false;
    QPointF origin;
    double progress = 0.0;
    double maxRadius = 0.0;
    double phase = 0.0;

    void trigger(const QPointF& pos, double w, double h) {
        origin = pos;
        progress = 0.02;
        phase = 0.0;
        double dx = qMax(pos.x(), w - pos.x());
        double dy = qMax(pos.y(), h - pos.y());
        maxRadius = qSqrt(dx * dx + dy * dy) * 1.15;
        active = true;
    }

    bool step(double dt) {
        if (!active) return false;
        progress += dt * 1.9;
        phase += dt * 9.0;
        if (progress >= 1.0) {
            active = false;
            progress = 0.0;
            return false;
        }
        return true;
    }

    void draw(QPainter& p) const {
        if (!active || maxRadius <= 0.0) return;
        double r = maxRadius * progress;
        int alpha = qBound(0, static_cast<int>((1.0 - progress) * 145.0), 255);

        QPainterPath path;
        constexpr int kSteps = 36;
        for (int i = 0; i <= kSteps; ++i) {
            double ang = (static_cast<double>(i) / kSteps) * M_PI * 2.0;
            double wobble = 1.0 + 0.045 * (1.0 - progress) * qSin(ang * 4.0 + phase);
            QPointF pt(origin.x() + qCos(ang) * r * wobble, origin.y() + qSin(ang) * r * wobble);
            if (i == 0) path.moveTo(pt);
            else path.lineTo(pt);
        }
        path.closeSubpath();

        QRadialGradient grad(origin, qMax(1.0, r));
        grad.setColorAt(0.0, QColor(255, 255, 255, alpha / 3));
        grad.setColorAt(0.72, QColor(0, 229, 255, alpha));
        grad.setColorAt(1.0, Qt::transparent);
        p.setBrush(grad);
        p.setPen(QPen(QColor(255, 255, 255, qMin(255, alpha + 30)), 1.6));
        p.drawPath(path);
    }
};

} // namespace directdrop
