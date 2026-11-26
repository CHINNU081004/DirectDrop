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
            bool b = m_stretch.step(dt);
            bool c = m_alpha.step(dt);
            update();
            if (!a && !b && !c) m_timer->stop();
        });
    }

    void moveLensTo(const QPointF& pos) {
        double dy = pos.y() - m_lastY;
        m_lastY = pos.y();
        m_pointer = pos;
        m_y.target = pos.y() - 21.0;
        m_stretch.target = qBound(-8.0, dy * 0.65, 8.0);
        m_alpha.target = 1.0;
        if (!m_timer->isActive()) m_timer->start();
    }

    void settleAndFade() {
        m_stretch.target = 0.0;
        m_alpha.target = 0.0;
        if (!m_timer->isActive()) m_timer->start();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (m_alpha.value <= 0.01) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        double extraH = qAbs(m_stretch.value);
        QRectF lensRect(10.0, m_y.value - extraH * 0.5, width() - 20.0, 42.0 + extraH);
        drawTransparentGlassLens(p, lensRect, 16.0, m_pointer, m_alpha.value);
    }

private:
    QTimer* m_timer = nullptr;
    QPointF m_pointer;
    double m_lastY = 0.0;
    Spring1D m_y{80.0, 0.0, 80.0, 260.0, 24.0};
    Spring1D m_stretch{0.0, 0.0, 0.0, 240.0, 20.0};
    Spring1D m_alpha{0.0, 0.0, 0.0, 180.0, 22.0};
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
        auto* sub = new QLabel(QStringLiteral("Wi-Fi Direct • v1.2.1"), this);
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
        for (int i = 0; i < m_buttons.size(); ++i) {
            m_buttons.at(i)->setChecked(i == idx);
        }
    }

signals:
    void navigationSelected(int index);

protected:
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (m_lens) m_lens->setGeometry(rect());
    }

    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::MouseMove) {
            if (auto* me = static_cast<QMouseEvent*>(ev)) {
                if (auto* w = qobject_cast<QWidget*>(obj)) {
                    m_lens->moveLensTo(w->mapTo(this, me->position()));
                }
            }
        }
        return QWidget::eventFilter(obj, ev);
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        m_lens->moveLensTo(e->position());
    }

    void leaveEvent(QEvent*) override {
        m_lens->settleAndFade();
    }

private:
    QList<QPushButton*> m_buttons;
    LiquidSidebarLens* m_lens = nullptr;
};

} // namespace directdrop
