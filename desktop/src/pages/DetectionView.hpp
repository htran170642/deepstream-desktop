#pragma once

#include <memory>

#include <QWidget>

#include "client/StreamClient.hpp"  // FrameUpdate

namespace dsd {

// Paints the decoded video frame (if any) and detection boxes + labels.
class DetectionView : public QWidget {
    Q_OBJECT

public:
    explicit DetectionView(QWidget* parent = nullptr);

    // Replace the displayed frame and repaint. nullptr clears the view.
    // Must be called on the UI thread.
    void setFrame(std::shared_ptr<const FrameUpdate> frame);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::shared_ptr<const FrameUpdate> frame_;
};

}  // namespace dsd
