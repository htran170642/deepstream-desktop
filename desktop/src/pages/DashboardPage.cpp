#include "pages/DashboardPage.hpp"

#include <cmath>

#include <QFormLayout>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace dsd {
namespace {

constexpr int kPollMs = 1000;

double toGiB(std::int64_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

int percent(std::int64_t used, std::int64_t total) {
    if (total <= 0) return 0;
    return static_cast<int>(std::lround(100.0 * static_cast<double>(used) /
                                        static_cast<double>(total)));
}

}  // namespace

DashboardPage::DashboardPage(QWidget* parent)
    : QWidget(parent), client_(std::make_shared<SystemClient>()) {
    auto* root = new QVBoxLayout(this);
    root->addWidget(new QLabel("<h2>Dashboard</h2>", this));

    auto* form = new QFormLayout();
    cpu_bar_ = new QProgressBar(this);
    mem_bar_ = new QProgressBar(this);
    gpu_bar_ = new QProgressBar(this);
    for (QProgressBar* bar : {cpu_bar_, mem_bar_, gpu_bar_}) bar->setRange(0, 100);
    form->addRow("CPU", cpu_bar_);
    form->addRow("Memory", mem_bar_);
    form->addRow("GPU", gpu_bar_);

    fps_label_ = new QLabel("—", this);
    pipeline_label_ = new QLabel("—", this);
    form->addRow("FPS", fps_label_);
    form->addRow("Pipeline", pipeline_label_);
    root->addLayout(form);

    camera_table_ = new QTableWidget(0, 4, this);
    camera_table_->setHorizontalHeaderLabels({"ID", "Name", "Enabled", "Running"});
    camera_table_->horizontalHeader()->setStretchLastSection(true);
    camera_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    camera_table_->setSelectionMode(QAbstractItemView::NoSelection);
    root->addWidget(camera_table_);

    status_label_ = new QLabel(this);
    status_label_->setWordWrap(true);
    root->addWidget(status_label_);

    watcher_ = std::make_unique<
        QFutureWatcher<std::optional<SystemStatsRecord>>>();
    connect(watcher_.get(),
            &QFutureWatcher<std::optional<SystemStatsRecord>>::finished, this,
            &DashboardPage::onStatsFetched);

    timer_ = new QTimer(this);
    timer_->setInterval(kPollMs);
    connect(timer_, &QTimer::timeout, this, &DashboardPage::poll);
    timer_->start();
    poll();  // first sample immediately
}

DashboardPage::~DashboardPage() {
    if (watcher_) watcher_->waitForFinished();
}

void DashboardPage::poll() {
    if (busy_) return;  // don't stack requests if a poll is still running
    busy_ = true;
    auto client = client_;  // capture the shared_ptr, not this
    watcher_->setFuture(QtConcurrent::run([client] { return client->stats(); }));
}

void DashboardPage::onStatsFetched() {
    busy_ = false;
    const auto result = watcher_->result();
    if (!result) {
        status_label_->setText("Service unreachable.");
        return;
    }
    const SystemStatsRecord& s = *result;

    cpu_bar_->setValue(static_cast<int>(std::lround(s.cpu_percent)));
    cpu_bar_->setFormat("%p%");

    const int mem_pct = percent(s.mem_used_bytes, s.mem_total_bytes);
    mem_bar_->setValue(mem_pct);
    mem_bar_->setFormat(QString("%1% (%2 / %3 GiB)")
                            .arg(mem_pct)
                            .arg(toGiB(s.mem_used_bytes), 0, 'f', 1)
                            .arg(toGiB(s.mem_total_bytes), 0, 'f', 1));

    if (s.gpu_available) {
        gpu_bar_->setEnabled(true);
        gpu_bar_->setValue(static_cast<int>(std::lround(s.gpu_percent)));
        gpu_bar_->setFormat(QString("%1% (%2 / %3 GiB)")
                                .arg(static_cast<int>(std::lround(s.gpu_percent)))
                                .arg(toGiB(s.gpu_mem_used_bytes), 0, 'f', 1)
                                .arg(toGiB(s.gpu_mem_total_bytes), 0, 'f', 1));
    } else {
        gpu_bar_->setEnabled(false);
        gpu_bar_->setValue(0);
        gpu_bar_->setFormat("N/A");
    }

    fps_label_->setText(QString::number(s.fps, 'f', 1));
    pipeline_label_->setText(
        s.pipeline_running
            ? QString("running — %1 camera(s)").arg(s.active_cameras)
            : QString("stopped"));

    camera_table_->setRowCount(0);
    for (const auto& c : s.cameras) {
        const int row = camera_table_->rowCount();
        camera_table_->insertRow(row);
        camera_table_->setItem(
            row, 0, new QTableWidgetItem(QString::number(c.id)));
        camera_table_->setItem(
            row, 1, new QTableWidgetItem(QString::fromStdString(c.name)));
        camera_table_->setItem(
            row, 2, new QTableWidgetItem(c.enabled ? "yes" : "no"));
        camera_table_->setItem(
            row, 3, new QTableWidgetItem(c.running ? "yes" : "no"));
    }
    status_label_->clear();
}

}  // namespace dsd
