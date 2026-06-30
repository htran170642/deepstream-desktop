#pragma once

#include <cstdint>
#include <memory>

#include <QWidget>

#include "client/StreamClient.hpp"

class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;

namespace dsd {

class DetectionView;

// Live View: pick a camera id, Start/Stop its pipeline, and watch detections
// drawn over a canvas with an FPS counter. (6b adds the decoded video frame.)
class LiveViewPage : public QWidget {
    Q_OBJECT

public:
    explicit LiveViewPage(QWidget* parent = nullptr);
    ~LiveViewPage() override;

private:
    void startSelected();
    void stopSelected();
    void onFrame(const FrameUpdate& frame);  // runs on the UI thread
    void updateFps();

    std::shared_ptr<StreamClient> client_;

    QSpinBox* camera_id_spin_ = nullptr;
    QPushButton* start_button_ = nullptr;
    QPushButton* stop_button_ = nullptr;
    QLabel* fps_label_ = nullptr;
    DetectionView* view_ = nullptr;
    QTimer* fps_timer_ = nullptr;

    std::int64_t active_camera_ = -1;
    int frame_count_ = 0;  // frames since the last FPS tick
};

}  // namespace dsd
