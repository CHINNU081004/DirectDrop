#pragma once

#include "LiquidGlassPhysics.hpp"

#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace directdrop {

class LiquidSidebarLens : public QWidget {
    Q_OBJECT
public:
    explicit LiquidSidebarLens(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        m_timer = new QTimer(this);
        m_timer->setInterval(16);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            constexpr double dt = 0.016;
            bool a = m_y.step(dt);
            bool b = m_h.step(dt);
            bool c = m_stretch.step(dt);
            bool d = m_alpha.step(dt);
            update();
            if (!a && !b && !c && !d) m_timer->stop();
        });
    }

    void trackTargetRect(const QRectF& targetRect, double dyVelocity = 0.0) {
        if (m_alpha.value <= 0.02) {
            // Initialize smoothly at target Y when entering from rest
            m_y.value = targetRect.top();
            m_h.value = targetRect.height();
        }
        m_y.target = targetRect.top();
        m_h.target = targetRect.height();
        m_stretch.target = qBound(-6.0, dyVelocity * 0.45, 6.0);
        m_alpha.target = 1.0;
        if (!m_timer->isActive()) m_timer->start();
    }

    void settleAndFade() {
        m_stretch.target = 0.0;
        m_alpha.target = 0.0;
        if (!m_timer->isActive()) m_timer->start();
    }

    int currentAlphaMilli() const { return static_cast<int>(m_alpha.value * 1000.0); }
    double currentY() const { return m_y.value; }

protected:
    void paintEvent(QPaintEvent*) override {
        if (m_alpha.value <= 0.01) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        double extraH = qAbs(m_stretch.value);
        QRectF lensRect(12.0, m_y.value - extraH * 0.5, width() - 24.0, m_h.value + extraH);
        drawTransparentGlassLens(p, lensRect, 14.0, QPointF(), m_alpha.value);
    }

private:
    QTimer* m_timer = nullptr;
    Spring1D m_y{80.0, 0.0, 80.0, 320.0, 26.0};
    Spring1D m_h{42.0, 0.0, 42.0, 320.0, 26.0};
    Spring1D m_stretch{0.0, 0.0, 0.0, 260.0, 22.0};
    Spring1D m_alpha{0.0, 0.0, 0.0, 220.0, 24.0};
};

class LiquidSidebar : public QWidget {
    Q_OBJECT
public:
    explicit LiquidSidebar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(210);
        setMouseTracking(true);
        setObjectName(QStringLiteral("liquidSidebar"));

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(12, 18, 12, 18);
        root->setSpacing(6);

        auto* brand = new QLabel(QStringLiteral("DirectDrop"), this);
        brand->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: #FFFFFF; padding: 4px 8px; background: transparent;"));
        auto* sub = new QLabel(QStringLiteral("Wi-Fi Direct • v1.4.0"), this);
        sub->setStyleSheet(QStringLiteral("font-size: 11px; color: rgba(255,255,255,0.65); padding: 0 8px 10px 8px; background: transparent;"));
        root->addWidget(brand);
        root->addWidget(sub);

        const QStringList labels = {
            QStringLiteral("Overview"),
            QStringLiteral("Files"),
            QStringLiteral("Connect Device"),
            QStringLiteral("Transfers"),
            QStringLiteral("Storage")
        };

        for (int i = 0; i < labels.size(); ++i) {
            auto* btn = new QPushButton(labels.at(i), this);
            btn->setCheckable(true);
            btn->setChecked(i == 0);
            btn->setMouseTracking(true);
            btn->installEventFilter(this);
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { text-align: left; padding: 10px 14px; border-radius: 14px; border: 1px solid transparent; background: transparent; color: rgba(255,255,255,0.82); font-weight: 600; min-height: 22px; }"
                "QPushButton:checked { background: rgba(255,255,255,0.12); border: 1px solid rgba(255,255,255,0.34); color: #FFFFFF; }"
            ));
            connect(btn, &QPushButton::clicked, this, [this, i]() {
                setActiveIndex(i);
                emit navigationSelected(i);
            });
            m_buttons.append(btn);
            root->addWidget(btn);
        }
        root->addStretch();

        m_lens = new LiquidSidebarLens(this);
        m_lens->raise();
    }

    void setActiveIndex(int idx) {
        m_activeIndex = idx;
        for (int i = 0; i < m_buttons.size(); ++i) {
            m_buttons.at(i)->setChecked(i == idx);
        }
    }

    int activeIndex() const { return m_activeIndex; }
    int hoveredIndex() const { return m_hoveredIndex; }
    LiquidSidebarLens* lensOverlay() const { return m_lens; }

    void setHoveredIndex(int idx, double dyVelocity = 0.0) {
        m_hoveredIndex = idx;
        if (idx >= 0 && idx < m_buttons.size()) {
            m_lens->trackTargetRect(QRectF(m_buttons.at(idx)->geometry()), dyVelocity);
        } else {
            m_lens->settleAndFade();
        }
    }

signals:
    void navigationSelected(int index);

protected:
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (m_lens) m_lens->setGeometry(rect());
        if (m_hoveredIndex >= 0 && m_hoveredIndex < m_buttons.size()) {
            m_lens->trackTargetRect(QRectF(m_buttons.at(m_hoveredIndex)->geometry()), 0.0);
        }
    }

    bool eventFilter(QObject* obj, QEvent* ev) override {
        int btnIdx = m_buttons.indexOf(qobject_cast<QPushButton*>(obj));
        if (btnIdx >= 0) {
            if (ev->type() == QEvent::Enter) {
                double dy = 0.0;
                if (m_hoveredIndex >= 0 && m_hoveredIndex < m_buttons.size()) {
                    dy = m_buttons.at(btnIdx)->geometry().top() - m_buttons.at(m_hoveredIndex)->geometry().top();
                }
                setHoveredIndex(btnIdx, dy * 0.25);
            } else if (ev->type() == QEvent::MouseMove) {
                if (m_hoveredIndex != btnIdx) {
                    setHoveredIndex(btnIdx, 0.0);
                }
            } else if (ev->type() == QEvent::Leave) {
                // Only clear hover if cursor actually left all buttons and sidebar
                QPoint localPos = mapFromGlobal(QCursor::pos());
                if (!rect().contains(localPos)) {
                    setHoveredIndex(-1);
                } else {
                    int under = buttonIndexAt(localPos);
                    if (under >= 0) setHoveredIndex(under, 0.0);
                }
            }
        }
        return QWidget::eventFilter(obj, ev);
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        int under = buttonIndexAt(e->position().toPoint());
        if (under >= 0 && under != m_hoveredIndex) {
            setHoveredIndex(under, 0.0);
        } else if (under < 0 && !rect().contains(e->position().toPoint())) {
            setHoveredIndex(-1);
        }
    }

    void leaveEvent(QEvent*) override {
        setHoveredIndex(-1);
    }

private:
    int buttonIndexAt(const QPoint& pt) const {
        for (int i = 0; i < m_buttons.size(); ++i) {
            if (m_buttons.at(i)->geometry().contains(pt)) return i;
        }
        return -1;
    }

    int m_activeIndex = 0;
    int m_hoveredIndex = -1;
    QList<QPushButton*> m_buttons;
    LiquidSidebarLens* m_lens = nullptr;
};

} // namespace directdrop
