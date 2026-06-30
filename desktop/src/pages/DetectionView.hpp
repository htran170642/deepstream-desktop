#pragma once

#include <QWidget>

#include "client/StreamClient.hpp"  // FrameUpdate

namespace dsd {

// Paints detection bounding boxes + labels over the view area. In 6a there is
// no video, so boxes are drawn on a dark canvas; 6b will paint the decoded
// frame first, then the boxes on top.
class DetectionView : public QWidget {
    Q_OBJECT

public:
    explicit DetectionView(QWidget* parent = nullptr);

    // Replace the displayed frame and repaint. Must be called on the UI thread.
    void setFrame(const FrameUpdate& frame);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    FrameUpdate frame_;
};

}  // namespace dsd
