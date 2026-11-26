#pragma once

#include <QLayout>
#include <QRect>
#include <QStyle>
#include <QWidget>
#include <QVector>

namespace directdrop {

/**
 * Responsive wrapping layout so toolbar buttons wrap onto additional rows
 * instead of clipping or truncating at narrower window widths.
 */
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = 0, int hSpacing = 8, int vSpacing = 8)
        : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing) {
        setContentsMargins(margin, margin, margin, margin);
    }

    ~FlowLayout() override {
        QLayoutItem* item = nullptr;
        while ((item = takeAt(0)) != nullptr) {
            delete item;
        }
    }

    void addItem(QLayoutItem* item) override { m_itemList.append(item); }
    int count() const override { return m_itemList.size(); }
    QLayoutItem* itemAt(int index) const override { return m_itemList.value(index); }
    QLayoutItem* takeAt(int index) override {
        if (index >= 0 && index < m_itemList.size()) return m_itemList.takeAt(index);
        return nullptr;
    }

    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }

    QSize minimumSize() const override {
        QSize size;
        for (const QLayoutItem* item : m_itemList) {
            size = size.expandedTo(item->minimumSize());
        }
        const QMargins m = contentsMargins();
        size += QSize(m.left() + m.right(), m.top() + m.bottom());
        return size;
    }

    QSize sizeHint() const override { return minimumSize(); }

    void setGeometry(const QRect& rect) override {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

private:
    int horizontalSpacing() const {
        if (m_hSpace >= 0) return m_hSpace;
        return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    }

    int verticalSpacing() const {
        if (m_vSpace >= 0) return m_vSpace;
        return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
    }

    int smartSpacing(QStyle::PixelMetric pm) const {
        QObject* p = parent();
        if (!p) return -1;
        if (p->isWidgetType()) {
            auto* pw = static_cast<QWidget*>(p);
            return pw->style()->pixelMetric(pm, nullptr, pw);
        }
        return static_cast<QLayout*>(p)->spacing();
    }

    int doLayout(const QRect& rect, bool testOnly) const {
        int left = 0, top = 0, right = 0, bottom = 0;
        getContentsMargins(&left, &top, &right, &bottom);
        QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
        int x = effectiveRect.x();
        int y = effectiveRect.y();
        int lineHeight = 0;
        const int spaceX = horizontalSpacing();
        const int spaceY = verticalSpacing();

        for (QLayoutItem* item : m_itemList) {
            const QWidget* wid = item->widget();
            if (wid && wid->isHidden()) continue;
            const QSize sz = item->sizeHint();
            int nextX = x + sz.width() + spaceX;
            if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
                x = effectiveRect.x();
                y = y + lineHeight + spaceY;
                nextX = x + sz.width() + spaceX;
                lineHeight = 0;
            }
            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y), sz));
            }
            x = nextX;
            lineHeight = qMax(lineHeight, sz.height());
        }
        return y + lineHeight - rect.y() + bottom;
    }

    QVector<QLayoutItem*> m_itemList;
    int m_hSpace;
    int m_vSpace;
};

} // namespace directdrop
