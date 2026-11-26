#pragma once

#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRadialGradient>
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
 * - Directional luminous rim highlight
 * - Small, natural specular glint following the interaction light position
 */
inline void drawTransparentGlassLens(
    QPainter& p,
    const QRectF& rect,
    double radius,
    const QPointF& lightPos,
    double alphaScale = 1.0
) {
    double a = qBound(0.0, alphaScale, 1.0);
    if (a <= 0.01) return;

    // 1. Subtle depth shadow
    p.setBrush(QColor(0, 0, 0, static_cast<int>(45 * a)));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect.adjusted(0, 2.0, 0, 3.0), radius, radius);

    // 2. Transparent neutral interior
    QLinearGradient fillGrad(rect.topLeft(), rect.bottomRight());
    fillGrad.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(34 * a)));
    fillGrad.setColorAt(1.0, QColor(255, 255, 255, static_cast<int>(12 * a)));
    p.setBrush(fillGrad);
    p.drawRoundedRect(rect, radius, radius);

    // 3. Directional luminous rim
    QLinearGradient rimGrad(lightPos - QPointF(rect.width() * 0.35, rect.height() * 0.35), rect.bottomRight());
    rimGrad.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(210 * a)));
    rimGrad.setColorAt(0.55, QColor(255, 255, 255, static_cast<int>(68 * a)));
    rimGrad.setColorAt(1.0, QColor(255, 255, 255, static_cast<int>(135 * a)));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QBrush(rimGrad), 1.4));
    p.drawRoundedRect(rect, radius, radius);

    // 4. Small natural specular glint
    double gx = qBound(rect.left() + 8.0, lightPos.x(), rect.right() - 8.0);
    double gy = qBound(rect.top() + 4.0, rect.top() + rect.height() * 0.28, rect.bottom() - 4.0);
    double gr = qMax(14.0, qMin(rect.width(), rect.height()) * 0.55);
    QRadialGradient glint(QPointF(gx, gy), gr);
    glint.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(115 * a)));
    glint.setColorAt(1.0, Qt::transparent);
    p.setBrush(glint);
    p.setPen(Qt::NoPen);
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
        p.setPen(QPen(QColor(255, 255, 255, qBound(0, static_cast<int>(45 * fade), 255)), 4.0));
        p.drawPath(path);

        p.setPen(QPen(QColor(255, 255, 255, qBound(0, static_cast<int>(185 * fade), 255)), 1.5));
        p.drawPath(path);
    }
};

} // namespace directdrop
