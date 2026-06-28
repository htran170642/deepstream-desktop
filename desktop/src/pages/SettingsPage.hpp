#pragma once

#include "pages/PlaceholderPage.hpp"

namespace dsd {

// Settings page placeholder. Filled in later with app configuration UI.
class SettingsPage : public PlaceholderPage {
public:
    explicit SettingsPage(QWidget* parent = nullptr);
};

}  // namespace dsd
