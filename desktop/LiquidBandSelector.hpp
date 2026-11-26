#pragma once

#include "LiquidSegmentedSelector.hpp"

namespace directdrop {

class LiquidBandSelector : public LiquidSegmentedSelector {
    Q_OBJECT
public:
    explicit LiquidBandSelector(QWidget* parent = nullptr)
        : LiquidSegmentedSelector(
              {QStringLiteral("Auto"), QStringLiteral("2.4 GHz"), QStringLiteral("5 GHz"), QStringLiteral("6 GHz")},
              parent) {
        connect(this, &LiquidSegmentedSelector::selectionChanged, this, &LiquidBandSelector::bandChanged);
    }

signals:
    void bandChanged(int index, const QString& label);
};

} // namespace directdrop
