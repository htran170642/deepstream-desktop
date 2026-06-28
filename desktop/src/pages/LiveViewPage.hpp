#pragma once

#include "pages/PlaceholderPage.hpp"

namespace dsd {

// Live View page placeholder. Phase 6 fills this with the video grid.
class LiveViewPage : public PlaceholderPage {
public:
    explicit LiveViewPage(QWidget* parent = nullptr);
};

}  // namespace dsd
