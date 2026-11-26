#pragma once

#include "LiquidGlassPhysics.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace directdrop {

class LiquidDeviceCard : public QWidget {
    Q_OBJECT
public:
    explicit LiquidDeviceCard(const QString& name, const QString& hwAddr, int strength, const QString& objectPath, QWidget* parent = nullptr)
        : QWidget(parent), m_objectPath(objectPath) {
        setMinimumHeight(68);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);

        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(18, 12, 18, 12);
        auto* textCol = new QVBoxLayout();
        auto* lblName = new QLabel(name.isEmpty() ? QStringLiteral("Wi-Fi Direct Device") : name, this);
        lblName->setStyleSheet(QStringLiteral("color: #FFFFFF; font-size: 14px; font-weight: 700; background: transparent;"));
        auto* lblMeta = new QLabel(QStringLiteral("%1 • Signal %2%").arg(hwAddr).arg(strength), this);
        lblMeta->setStyleSheet(QStringLiteral("color: #90CAF9; font-size: 12px; background: transparent;"));
        textCol->addWidget(lblName);
        textCol->addWidget(lblMeta);
        lay->addLayout(textCol, 1);

        m_lblAction = new QLabel(QStringLiteral("Connect"), this);
        m_lblAction->setStyleSheet(QStringLiteral("color: #00E5FF; font-weight: 700; background: transparent;"));
        lay->addWidget(m_lblAction);

        m_timer = new QTimer(this);
        m_timer->setInterval(16);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            constexpr double dt = 0.016;
            bool a = m_press.step(dt);
            bool b = m_ripple.step(dt);
            update();
            if (!a && !b) m_timer->stop();
        });
    }

    QString objectPath() const { return m_objectPath; }
    void setSelected(bool sel) {
        m_selected = sel;
        m_lblAction->setText(sel ? QStringLiteral("Selected") : QStringLiteral("Connect"));
        update();
    }

signals:
    void selectedWithOrigin(const QString& objectPath, const QPointF& origin);

protected:
    void enterEvent(QEnterEvent*) override { m_hovered = true; update(); }
    void leaveEvent(QEvent*) override { m_hovered = false; update(); }

    void mousePressEvent(QMouseEvent* e) override {
        m_pointer = e->position();
        m_press.target = 1.0;
        m_ripple.trigger(e->position(), width(), height());
        if (!m_timer->isActive()) m_timer->start();
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        m_pointer = e->position();
        if (m_hovered) update();
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        m_press.target = 0.0;
        if (!m_timer->isActive()) m_timer->start();
        setSelected(true);
        emit selectedWithOrigin(m_objectPath, e->position());
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        double inset = m_press.value * 2.5;
        QRectF r = rect().adjusted(2 + inset, 2 + inset, -2 - inset, -2 - inset);
        QPointF lightPos = m_hovered ? m_pointer : QPointF(r.center().x(), r.top() + r.height() * 0.25);
        drawTransparentGlassLens(p, r, 16.0, lightPos, m_selected ? 1.0 : (m_hovered ? 0.85 : 0.60));

        m_ripple.draw(p);

        if (m_selected) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 255, 255, 215), 1.8));
            p.drawRoundedRect(r, 16.0, 16.0);
        }
    }

private:
    QString m_objectPath;
    QLabel* m_lblAction = nullptr;
    QTimer* m_timer = nullptr;
    bool m_hovered = false;
    bool m_selected = false;
    QPointF m_pointer;
    Spring1D m_press{0.0};
    LiquidRipple m_ripple;
};

} // namespace directdrop
