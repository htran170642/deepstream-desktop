#pragma once

#include "pages/PlaceholderPage.hpp"

namespace dsd {

// Camera page placeholder. Phase 4 fills this with the camera CRUD UI.
class CameraPage : public PlaceholderPage {
public:
    explicit CameraPage(QWidget* parent = nullptr);
};

}  // namespace dsd
