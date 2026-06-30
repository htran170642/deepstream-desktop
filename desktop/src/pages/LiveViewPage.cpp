#include "pages/LiveViewPage.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include "pages/DetectionView.hpp"

namespace dsd {

LiveViewPage::LiveViewPage(QWidget* parent)
    : QWidget(parent), client_(std::make_shared<StreamClient>()) {
    auto* layout = new QVBoxLayout(this);

    // Controls row.
    auto* controls = new QHBoxLayout();
    controls->addWidget(new QLabel("Camera id:", this));
    camera_id_spin_ = new QSpinBox(this);
    camera_id_spin_->setRange(1, 1000000);
    controls->addWidget(camera_id_spin_);
    start_button_ = new QPushButton("Start", this);
    stop_button_ = new QPushButton("Stop", this);
    stop_button_->setEnabled(false);
    controls->addWidget(start_button_);
    controls->addWidget(stop_button_);
    controls->addStretch();
    fps_label_ = new QLabel("FPS: 0", this);
    controls->addWidget(fps_label_);
    layout->addLayout(controls);

    // Detection canvas.
    view_ = new DetectionView(this);
    layout->addWidget(view_, 1);

    connect(start_button_, &QPushButton::clicked, this,
            &LiveViewPage::startSelected);
    connect(stop_button_, &QPushButton::clicked, this,
            &LiveViewPage::stopSelected);

    fps_timer_ = new QTimer(this);
    fps_timer_->setInterval(1000);
    connect(fps_timer_, &QTimer::timeout, this, &LiveViewPage::updateFps);
}

LiveViewPage::~LiveViewPage() {
    client_->unsubscribe();  // join the stream worker before this widget dies
}

void LiveViewPage::startSelected() {
    const std::int64_t id = camera_id_spin_->value();

    if (!client_->startCamera(id)) {
        QMessageBox::warning(this, "Start failed",
                             "Could not start the camera pipeline.");
        return;
    }

    active_camera_ = id;
    frame_count_ = 0;

    // Stream runs on a worker thread; marshal each frame to the UI thread.
    // Using `this` as the context makes Qt drop pending events if we're gone.
    client_->subscribe(id, [this](std::shared_ptr<const FrameUpdate> f) {
        QMetaObject::invokeMethod(
            this, [this, f] { onFrame(f); }, Qt::QueuedConnection);
    });


    start_button_->setEnabled(false);
    stop_button_->setEnabled(true);
    fps_timer_->start();
}

void LiveViewPage::stopSelected() {
    client_->unsubscribe();
    if (active_camera_ >= 0) {
        client_->stopCamera(active_camera_);
        active_camera_ = -1;
    }
    fps_timer_->stop();
    fps_label_->setText("FPS: 0");
    view_->setFrame(nullptr);  // clear the canvas
    start_button_->setEnabled(true);
    stop_button_->setEnabled(false);
}

void LiveViewPage::onFrame(std::shared_ptr<const FrameUpdate> frame) {
    if (active_camera_ < 0) {
        return;  // a frame queued before Stop arrived late — ignore it
    }
    view_->setFrame(std::move(frame));
    ++frame_count_;
}

void LiveViewPage::updateFps() {
    fps_label_->setText(QString("FPS: %1").arg(frame_count_));
    frame_count_ = 0;
}

}  // namespace dsd
