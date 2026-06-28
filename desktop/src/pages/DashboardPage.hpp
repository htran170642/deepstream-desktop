#pragma once

#include "pages/PlaceholderPage.hpp"

namespace dsd {

// Dashboard page placeholder. Phase 9 fills this with system metrics.
class DashboardPage : public PlaceholderPage {
public:
    explicit DashboardPage(QWidget* parent = nullptr);
};

}  // namespace dsd
