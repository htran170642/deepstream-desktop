#include "pages/DetectionView.hpp"

#include <QColor>
#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QString>

namespace dsd {

DetectionView::DetectionView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 240);
}

void DetectionView::setFrame(const FrameUpdate& frame) {
    frame_ = frame;
    update();  // schedule a repaint on the UI thread
}

void DetectionView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    const QRect area = rect();

    // Dark canvas (6b will paint the decoded video frame here instead).
    painter.fillRect(area, QColor(20, 20, 20));

    const qreal w = area.width();
    const qreal h = area.height();

    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);

    for (const DetectionBox& d : frame_.detections) {
        // Normalized [0,1] -> widget pixels.
        const QRectF box(d.x * w, d.y * h, d.width * w, d.height * h);

        painter.setPen(QPen(QColor(0, 200, 0), 2));
        painter.drawRect(box);

        // Label chip: "person 0.92"
        const QString label = QString("%1 %2")
                                  .arg(QString::fromStdString(d.label))
                                  .arg(d.confidence, 0, 'f', 2);
        const QRectF chip(box.left(), box.top() - 16, box.width(), 16);
        painter.fillRect(chip, QColor(0, 200, 0, 160));
        painter.setPen(Qt::black);
        painter.drawText(box.left() + 2, box.top() - 3, label);
    }

    if (frame_.detections.empty()) {
        painter.setPen(Qt::gray);
        painter.drawText(area, Qt::AlignCenter,
                         "No detections (start a camera and select it)");
    }
}

}  // namespace dsd
