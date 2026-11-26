#pragma once

#include "LiquidGlassPhysics.hpp"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace directdrop {

/**
 * Reusable translucent liquid-glass segmented selector for desktop.
 * Supports arbitrary segment labels, proportional/content-aware widths,
 * spring-animated lens motion, hover/focus distinction, and reduced motion.
 */
class LiquidSegmentedSelector : public QWidget {
    Q_OBJECT
public:
    explicit LiquidSegmentedSelector(QStringList labels, QWidget* parent = nullptr)
        : QWidget(parent), m_labels(std::move(labels)) {
        setMinimumHeight(40);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        updateMinimumWidth();

        m_timer = new QTimer(this);
        m_timer->setInterval(16);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            constexpr double dt = 0.016;
            bool a = m_lensX.step(dt);
            bool b = m_lensW.step(dt);
            bool c = m_press.step(dt);
            bool d = m_stretch.step(dt);
            update();
            if (!a && !b && !c && !d && !m_dragging) m_timer->stop();
        });
    }

    int currentIndex() const { return m_selected; }
    QString currentText() const { return m_labels.value(m_selected); }
    int count() const { return m_labels.size(); }

    void setCurrentIndex(int idx, bool notify = false) {
        if (m_labels.isEmpty()) return;
        int clamped = qBound(0, idx, m_labels.size() - 1);
        bool changed = (clamped != m_selected);
        m_selected = clamped;
        syncTargetsToSelected(!style()->styleHint(QStyle::SH_Widget_Animate));
        if (changed && notify) emit selectionChanged(m_selected, m_labels.at(m_selected));
        update();
    }

    QSize sizeHint() const override {
        return QSize(qMax(220, m_minWidth), 40);
    }

    QSize minimumSizeHint() const override {
        return QSize(qMax(180, m_minWidth), 38);
    }

signals:
    void selectionChanged(int index, const QString& label);

protected:
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        recomputeGeometry();
        syncTargetsToSelected(true);
    }

    void leaveEvent(QEvent*) override {
        if (m_hoverIndex != -1) {
            m_hoverIndex = -1;
            update();
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (m_labels.isEmpty()) return;
        m_dragging = true;
        m_lastX = e->position().x();
        m_press.target = 1.0;
        if (style()->styleHint(QStyle::SH_Widget_Animate)) {
            if (!m_timer->isActive()) m_timer->start();
        } else {
            m_press.value = 1.0;
            update();
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (m_labels.isEmpty()) return;
        int hovered = indexAtX(e->position().x());
        if (hovered != m_hoverIndex) {
            m_hoverIndex = hovered;
            if (!m_dragging) update();
        }
        if (m_dragging) {
            double dx = e->position().x() - m_lastX;
            m_lastX = e->position().x();
            int idx = indexAtX(e->position().x());
            if (idx >= 0 && idx < m_segRects.size()) {
                m_lensX.target = m_segRects[idx].left();
                m_lensW.target = m_segRects[idx].width();
                m_stretch.target = qBound(-0.32, dx / 14.0, 0.32);
                if (!m_timer->isActive()) m_timer->start();
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        m_dragging = false;
        m_press.target = 0.0;
        m_stretch.target = 0.0;
        int idx = indexAtX(e->position().x());
        setCurrentIndex(idx, true);
    }

    void keyPressEvent(QKeyEvent* e) override {
        if (m_labels.isEmpty()) {
            QWidget::keyPressEvent(e);
            return;
        }
        if (e->key() == Qt::Key_Left) {
            setCurrentIndex(qMax(0, m_selected - 1), true);
            return;
        }
        if (e->key() == Qt::Key_Right) {
            setCurrentIndex(qMin(m_labels.size() - 1, m_selected + 1), true);
            return;
        }
        QWidget::keyPressEvent(e);
    }

    void paintEvent(QPaintEvent*) override {
        if (m_segRects.size() != m_labels.size()) recomputeGeometry();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRectF track = rect().adjusted(2, 2, -2, -2);
        double r = track.height() / 2.0;
        p.setBrush(QColor(255, 255, 255, 22));
        p.setPen(QPen(QColor(255, 255, 255, hasFocus() ? 140 : 78), hasFocus() ? 1.5 : 1.2));
        p.drawRoundedRect(track, r, r);

        // Subtle hover pill (kept distinct from selected lens)
        if (m_hoverIndex >= 0 && m_hoverIndex < m_segRects.size() && m_hoverIndex != m_selected) {
            QRectF hRect = m_segRects[m_hoverIndex].adjusted(3, 3, -3, -3);
            p.setBrush(QColor(255, 255, 255, 16));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(hRect, hRect.height() / 2.0, hRect.height() / 2.0);
        }

        if (!m_segRects.isEmpty()) {
            double stretchPx = qAbs(m_stretch.value) * m_lensW.value * 0.22;
            double compY = m_press.value * 2.2;
            double left = m_lensX.value + 3.0 - (m_stretch.value < 0.0 ? stretchPx : 0.0);
            double right = m_lensX.value + m_lensW.value - 3.0 + (m_stretch.value > 0.0 ? stretchPx : 0.0);
            left = qMax(track.left() + 2.0, left);
            right = qMin(track.right() - 2.0, right);
            QRectF thumb(left, track.top() + 2.0 + compY,
                         qMax(24.0, right - left),
                         qMax(16.0, track.height() - 4.0 - compY * 2.0));
            drawTransparentGlassLens(p, thumb, thumb.height() / 2.0, QPointF(), 1.0);
        }

        QFont f = font();
        f.setPointSize(9);
        f.setBold(true);
        p.setFont(f);
        for (int i = 0; i < m_labels.size(); ++i) {
            if (i == m_selected) p.setPen(Qt::white);
            else if (i == m_hoverIndex) p.setPen(QColor(255, 255, 255, 235));
            else p.setPen(QColor(226, 232, 240, 190));
            p.drawText(m_segRects[i], Qt::AlignCenter, m_labels.at(i));
        }
    }

private:
    void updateMinimumWidth() {
        QFont f = font();
        f.setPointSize(9);
        f.setBold(true);
        QFontMetrics fm(f);
        int total = 8;
        for (const QString& lbl : m_labels) {
            total += fm.horizontalAdvance(lbl) + 28;
        }
        m_minWidth = total;
        setMinimumWidth(m_minWidth);
    }

    void recomputeGeometry() {
        m_segRects.clear();
        if (m_labels.isEmpty()) return;
        QRectF track = rect().adjusted(2, 2, -2, -2);
        QFont f = font();
        f.setPointSize(9);
        f.setBold(true);
        QFontMetrics fm(f);

        QVector<double> weights;
        double sumW = 0.0;
        for (const QString& lbl : m_labels) {
            double w = fm.horizontalAdvance(lbl) + 28.0;
            weights.append(w);
            sumW += w;
        }
        double x = track.left();
        for (int i = 0; i < m_labels.size(); ++i) {
            double w = (sumW > 0.0) ? (track.width() * (weights[i] / sumW)) : (track.width() / m_labels.size());
            if (i == m_labels.size() - 1) w = track.right() - x;
            m_segRects.append(QRectF(x, track.top(), w, track.height()));
            x += w;
        }
    }

    void syncTargetsToSelected(bool immediate) {
        if (m_selected < 0 || m_selected >= m_segRects.size()) return;
        const QRectF& r = m_segRects[m_selected];
        m_lensX.target = r.left();
        m_lensW.target = r.width();
        if (immediate) {
            m_lensX.value = r.left();
            m_lensX.velocity = 0.0;
            m_lensW.value = r.width();
            m_lensW.velocity = 0.0;
            update();
        } else if (!m_timer->isActive()) {
            m_timer->start();
        }
    }

    int indexAtX(double x) const {
        for (int i = 0; i < m_segRects.size(); ++i) {
            if (x >= m_segRects[i].left() && x <= m_segRects[i].right()) return i;
        }
        return x < width() / 2.0 ? 0 : qMax(0, m_labels.size() - 1);
    }

    QStringList m_labels;
    QVector<QRectF> m_segRects;
    int m_selected = 0;
    int m_hoverIndex = -1;
    int m_minWidth = 220;
    bool m_dragging = false;
    double m_lastX = 0.0;
    QTimer* m_timer = nullptr;
    Spring1D m_lensX{0.0, 0.0, 0.0, 290.0, 24.0};
    Spring1D m_lensW{80.0, 0.0, 80.0, 290.0, 24.0};
    Spring1D m_press{0.0, 0.0, 0.0, 340.0, 24.0};
    Spring1D m_stretch{0.0, 0.0, 0.0, 260.0, 20.0};
};

} // namespace directdrop
