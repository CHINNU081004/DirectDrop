#pragma once

#include "LiquidGlassPhysics.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWidget>

namespace directdrop {

class LiquidBandSelector : public QWidget {
    Q_OBJECT
public:
    explicit LiquidBandSelector(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(40);
        setMinimumWidth(240);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);

        m_timer = new QTimer(this);
        m_timer->setInterval(16);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            constexpr double dt = 0.016;
            bool a = m_thumb.step(dt);
            bool b = m_press.step(dt);
            bool c = m_stretch.step(dt);
            update();
            if (!a && !b && !c && !m_dragging) m_timer->stop();
        });
    }

    int currentIndex() const { return m_selected; }
    QString currentText() const { return m_labels.value(m_selected); }

signals:
    void bandChanged(int index, const QString& label);

protected:
    void mousePressEvent(QMouseEvent* e) override {
        m_dragging = true;
        m_lastX = e->position().x();
        m_pointer = e->position();
        m_press.target = 1.0;
        if (!m_timer->isActive()) m_timer->start();
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        m_pointer = e->position();
        if (m_dragging) {
            double dx = e->position().x() - m_lastX;
            m_lastX = e->position().x();
            double norm = qBound(0.0, (e->position().x() / qMax(1.0, static_cast<double>(width()))) * 3.0 - 0.5, 2.0);
            m_thumb.target = norm;
            m_stretch.target = qBound(-0.35, dx / 14.0, 0.35);
            if (!m_timer->isActive()) m_timer->start();
        } else {
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        m_dragging = false;
        m_press.target = 0.0;
        m_stretch.target = 0.0;
        double segW = width() / 3.0;
        int idx = qBound(0, static_cast<int>(e->position().x() / qMax(1.0, segW)), 2);
        if (idx != m_selected) {
            m_selected = idx;
            emit bandChanged(idx, m_labels.at(idx));
        }
        m_thumb.target = static_cast<double>(idx);
        if (!m_timer->isActive()) m_timer->start();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRectF track = rect().adjusted(2, 2, -2, -2);
        double r = track.height() / 2.0;
        p.setBrush(QColor(255, 255, 255, 26));
        p.setPen(QPen(QColor(255, 255, 255, 80), 1.2));
        p.drawRoundedRect(track, r, r);

        double segW = track.width() / 3.0;
        double stretchPx = qAbs(m_stretch.value) * segW * 0.25;
        double compY = m_press.value * 2.5;
        double left = track.left() + m_thumb.value * segW + 3.0 - (m_stretch.value < 0.0 ? stretchPx : 0.0);
        double right = left + segW - 6.0 + stretchPx;
        QRectF thumb(qMax(track.left() + 2.0, left), track.top() + 2.0 + compY,
                     qMin(track.right() - 2.0, right) - qMax(track.left() + 2.0, left),
                     track.height() - 4.0 - compY * 2.0);

        double hlX = m_dragging ? qBound(thumb.left(), m_pointer.x(), thumb.right()) : thumb.center().x();
        double hlY = m_dragging ? qBound(thumb.top(), m_pointer.y(), thumb.bottom()) : thumb.top() + thumb.height() * 0.28;
        drawTransparentGlassLens(p, thumb, thumb.height() / 2.0, QPointF(hlX, hlY), 1.0);

        p.setFont(QFont(font().family(), 9, QFont::Bold));
        for (int i = 0; i < 3; ++i) {
            p.setPen(i == m_selected ? Qt::white : QColor(227, 242, 253, 195));
            QRectF cell(track.left() + i * segW, track.top(), segW, track.height());
            p.drawText(cell, Qt::AlignCenter, m_labels.at(i));
        }
    }

private:
    QStringList m_labels{QStringLiteral("Auto"), QStringLiteral("2.4 GHz"), QStringLiteral("5 GHz")};
    int m_selected = 0;
    bool m_dragging = false;
    double m_lastX = 0.0;
    QPointF m_pointer;
    QTimer* m_timer = nullptr;
    Spring1D m_thumb{0.0};
    Spring1D m_press{0.0};
    Spring1D m_stretch{0.0};
};

} // namespace directdrop
