#pragma once

#include "pages/PlaceholderPage.hpp"

namespace dsd {

// Alert page placeholder. Phase 7 fills this with the alert history UI.
class AlertPage : public PlaceholderPage {
public:
    explicit AlertPage(QWidget* parent = nullptr);
};

}  // namespace dsd
