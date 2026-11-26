#pragma once

#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
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

/**
 * Renders a predominantly transparent, colorless dimensional glass lens:
 * - Soft depth shadow underneath
 * - Transparent neutral glass interior (background remains visible)
 * - Subtle fixed top-edge sheen (no moving interior hotspot)
 * - Restrained luminous rim highlight
 */
inline void drawTransparentGlassLens(
    QPainter& p,
    const QRectF& rect,
    double radius,
    const QPointF& /*lightPos*/ = QPointF(),
    double alphaScale = 1.0
) {
    double a = qBound(0.0, alphaScale, 1.0);
    if (a <= 0.01) return;

    // 1. Subtle depth shadow
    p.setBrush(QColor(0, 0, 0, static_cast<int>(38 * a)));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect.adjusted(0, 2.0, 0, 3.0), radius, radius);

    // 2. Transparent neutral interior
    QLinearGradient fillGrad(rect.topLeft(), rect.bottomRight());
    fillGrad.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(26 * a)));
    fillGrad.setColorAt(1.0, QColor(255, 255, 255, static_cast<int>(10 * a)));
    p.setBrush(fillGrad);
    p.drawRoundedRect(rect, radius, radius);

    // 3. Subtle fixed top-edge inner sheen (no moving radial spot)
    QLinearGradient sheenGrad(rect.topLeft(), QPointF(rect.left(), rect.top() + rect.height() * 0.45));
    sheenGrad.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(30 * a)));
    sheenGrad.setColorAt(1.0, Qt::transparent);
    p.setBrush(sheenGrad);
    p.drawRoundedRect(rect, radius, radius);

    // 4. Restrained, stable directional rim highlight
    QLinearGradient rimGrad(rect.topLeft(), rect.bottomRight());
    rimGrad.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(150 * a)));
    rimGrad.setColorAt(0.5, QColor(255, 255, 255, static_cast<int>(52 * a)));
    rimGrad.setColorAt(1.0, QColor(255, 255, 255, static_cast<int>(100 * a)));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QBrush(rimGrad), 1.3));
    p.drawRoundedRect(rect, radius, radius);
}

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
        maxRadius = qSqrt(dx * dx + dy * dy) * 1.12;
        active = true;
    }

    bool step(double dt) {
        if (!active) return false;
        progress += dt * 1.85;
        phase += dt * 7.5;
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
        double fade = 1.0 - progress;

        QPainterPath path;
        constexpr int kSteps = 40;
        for (int i = 0; i <= kSteps; ++i) {
            double ang = (static_cast<double>(i) / kSteps) * M_PI * 2.0;
            double wobble = 1.0 + 0.035 * fade * qSin(ang * 4.0 + phase);
            QPointF pt(origin.x() + qCos(ang) * r * wobble, origin.y() + qSin(ang) * r * wobble);
            if (i == 0) path.moveTo(pt);
            else path.lineTo(pt);
        }
        path.closeSubpath();

        // Transparent interior — caustic glass band + crisp rim
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, qBound(0, static_cast<int>(42 * fade), 255)), 3.5));
        p.drawPath(path);

        p.setPen(QPen(QColor(255, 255, 255, qBound(0, static_cast<int>(165 * fade), 255)), 1.4));
        p.drawPath(path);
    }
};

} // namespace directdrop
