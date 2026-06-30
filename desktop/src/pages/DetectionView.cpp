#include "pages/DetectionView.hpp"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QString>

namespace dsd {

DetectionView::DetectionView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 240);
}

void DetectionView::setFrame(std::shared_ptr<const FrameUpdate> frame) {
    frame_ = std::move(frame);
    update();  // schedule a repaint on the UI thread
}

void DetectionView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    const QRect area = rect();

    // Background: the decoded video frame if present, else a dark canvas.
    painter.fillRect(area, QColor(20, 20, 20));
    if (frame_ && !frame_->jpeg.empty()) {
        const QImage image = QImage::fromData(
            frame_->jpeg.data(), static_cast<int>(frame_->jpeg.size()), "JPEG");
        if (!image.isNull()) {
            painter.drawImage(area, image, image.rect());  // stretch to widget
        }
    }

    if (!frame_) {
        painter.setPen(Qt::gray);
        painter.drawText(area, Qt::AlignCenter, "No stream (start a camera)");
        return;
    }

    // Boxes + labels, normalized [0,1] -> widget pixels.
    const qreal w = area.width();
    const qreal h = area.height();

    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);

    for (const DetectionBox& d : frame_->detections) {
        const QRectF box(d.x * w, d.y * h, d.width * w, d.height * h);

        painter.setPen(QPen(QColor(0, 200, 0), 2));
        painter.drawRect(box);

        const QString label = QString("%1 %2")
                                  .arg(QString::fromStdString(d.label))
                                  .arg(d.confidence, 0, 'f', 2);
        const QRectF chip(box.left(), box.top() - 16, box.width(), 16);
        painter.fillRect(chip, QColor(0, 200, 0, 160));
        painter.setPen(Qt::black);
        painter.drawText(box.left() + 2, box.top() - 3, label);
    }
}

}  // namespace dsd
