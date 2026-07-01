#include "pages/AlertPage.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDateTime>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace dsd {
namespace {

QString formatTime(std::int64_t ms) {
    return QDateTime::fromMSecsSinceEpoch(ms).toString("yyyy-MM-dd HH:mm:ss");
}

// Write one alert into an existing table row. Shared by the full rebuild and
// the live-prepend path so the column layout lives in one place.
void fillRow(QTableWidget* table, int row, const AlertRecord& a) {
    table->setItem(row, 0, new QTableWidgetItem(formatTime(a.timestamp_ms)));
    table->setItem(row, 1,
                   new QTableWidgetItem(QString::number(a.camera_id)));
    table->setItem(row, 2,
                   new QTableWidgetItem(QString::fromStdString(a.label)));
    table->setItem(row, 3,
                   new QTableWidgetItem(QString::number(a.confidence, 'f', 2)));
    table->setItem(row, 4,
                   new QTableWidgetItem(a.has_snapshot ? "Yes" : "No"));
}

}  // namespace

AlertPage::AlertPage(QWidget* parent)
    : QWidget(parent), client_(std::make_shared<AlertClient>()) {
    auto* layout = new QVBoxLayout(this);

    // Filter / search row.
    auto* filters = new QHBoxLayout();
    filters->addWidget(new QLabel("Camera id:", this));
    camera_id_spin_ = new QSpinBox(this);
    camera_id_spin_->setRange(0, 1000000);
    camera_id_spin_->setSpecialValueText("Any");  // shown when value == 0
    filters->addWidget(camera_id_spin_);

    filters->addWidget(new QLabel("Label:", this));
    label_edit_ = new QLineEdit(this);
    label_edit_->setPlaceholderText("any");
    filters->addWidget(label_edit_);

    filters->addWidget(new QLabel("Limit:", this));
    limit_spin_ = new QSpinBox(this);
    limit_spin_->setRange(1, 1000);
    limit_spin_->setValue(100);
    filters->addWidget(limit_spin_);

    refresh_button_ = new QPushButton("Refresh", this);
    filters->addWidget(refresh_button_);
    live_check_ = new QCheckBox("Live", this);
    filters->addWidget(live_check_);
    filters->addStretch();
    layout->addLayout(filters);

    // Content: history table on the left, snapshot preview on the right.
    auto* content = new QHBoxLayout();
    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels(
        {"Time", "Camera", "Label", "Confidence", "Snapshot"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    content->addWidget(table_, 2);

    snapshot_label_ = new QLabel("No selection", this);
    snapshot_label_->setAlignment(Qt::AlignCenter);
    snapshot_label_->setMinimumSize(320, 240);
    snapshot_label_->setFrameShape(QFrame::StyledPanel);
    content->addWidget(snapshot_label_, 1);
    layout->addLayout(content, 1);

    status_label_ = new QLabel(this);
    status_label_->setStyleSheet("color: gray;");
    layout->addWidget(status_label_);

    // Async result watchers (unary calls run off the UI thread).
    list_watcher_ = std::make_unique<
        QFutureWatcher<std::optional<std::vector<AlertRecord>>>>();
    snapshot_watcher_ = std::make_unique<
        QFutureWatcher<std::optional<std::vector<std::uint8_t>>>>();

    connect(refresh_button_, &QPushButton::clicked, this, &AlertPage::refresh);
    connect(live_check_, &QCheckBox::toggled, this, &AlertPage::toggleLive);
    connect(table_, &QTableWidget::itemSelectionChanged, this,
            &AlertPage::onSelectionChanged);
    connect(list_watcher_.get(),
            &QFutureWatcher<std::optional<std::vector<AlertRecord>>>::finished,
            this, &AlertPage::onListed);
    connect(snapshot_watcher_.get(),
            &QFutureWatcher<std::optional<std::vector<std::uint8_t>>>::finished,
            this, &AlertPage::onSnapshotFetched);

    refresh();  // load the initial history
}

AlertPage::~AlertPage() {
    client_->unsubscribe();  // join the stream worker before this widget dies
}

void AlertPage::refresh() {
    setBusy(true);
    AlertQuery query;
    query.camera_id = camera_id_spin_->value();  // 0 = any
    query.label = label_edit_->text().toStdString();
    query.limit = limit_spin_->value();

    auto client = client_;  // shared_ptr copy: worker outlives the page safely
    list_watcher_->setFuture(
        QtConcurrent::run([client, query] { return client->list(query); }));
}

void AlertPage::onListed() {
    const auto result = list_watcher_->result();
    setBusy(false);

    if (!result) {
        status_label_->setText("Service unavailable — could not load alerts.");
        return;
    }
    alerts_ = *result;
    populateTable();
    status_label_->setText(alerts_.empty() ? "No alerts match the filter." : "");
}

void AlertPage::populateTable() {
    table_->setRowCount(static_cast<int>(alerts_.size()));
    for (int row = 0; row < static_cast<int>(alerts_.size()); ++row) {
        fillRow(table_, row, alerts_[row]);
    }
}

void AlertPage::onSelectionChanged() {
    const std::optional<AlertRecord> sel = selectedAlert();
    if (!sel) {
        snapshot_for_id_ = -1;
        snapshot_label_->setText("No selection");
        return;
    }
    if (!sel->has_snapshot) {
        snapshot_for_id_ = -1;
        snapshot_label_->setText("No snapshot");
        return;
    }
    snapshot_for_id_ = sel->id;
    snapshot_label_->setText("Loading…");

    auto client = client_;
    const std::int64_t id = sel->id;
    snapshot_watcher_->setFuture(
        QtConcurrent::run([client, id] { return client->snapshot(id); }));
}

void AlertPage::onSnapshotFetched() {
    const std::optional<AlertRecord> sel = selectedAlert();
    // The selection moved on while the fetch was in flight — drop the result.
    if (!sel || sel->id != snapshot_for_id_) {
        return;
    }
    const auto result = snapshot_watcher_->result();
    if (!result) {
        snapshot_label_->setText("Snapshot unavailable");
        return;
    }
    if (result->empty()) {
        snapshot_label_->setText("No snapshot");
        return;
    }
    QImage image = QImage::fromData(result->data(),
                                    static_cast<int>(result->size()), "JPEG");
    if (image.isNull()) {
        snapshot_label_->setText("Snapshot decode failed");
        return;
    }
    snapshot_label_->setPixmap(QPixmap::fromImage(image).scaled(
        snapshot_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void AlertPage::toggleLive(bool on) {
    if (!on) {
        client_->unsubscribe();
        return;
    }
    // Stream runs on a worker thread; marshal each alert to the UI thread.
    client_->subscribe([this](std::shared_ptr<const AlertRecord> a) {
        QMetaObject::invokeMethod(
            this, [this, a] { onLiveAlert(a); }, Qt::QueuedConnection);
    });
}

void AlertPage::onLiveAlert(std::shared_ptr<const AlertRecord> alert) {
    // Cap the live feed so a long-running subscription can't grow the table (and
    // its backing vector) without bound — the history path is capped by `limit`,
    // this path needs its own bound.
    constexpr int kMaxLiveRows = 500;

    // Newest first: prepend to both the model and the table so the current
    // selection (and its row index) stays aligned with alerts_.
    alerts_.insert(alerts_.begin(), *alert);
    table_->insertRow(0);
    fillRow(table_, 0, *alert);

    // Drop the oldest rows past the cap (both stay in lock-step, oldest last).
    while (static_cast<int>(alerts_.size()) > kMaxLiveRows) {
        alerts_.pop_back();
        table_->removeRow(table_->rowCount() - 1);
    }
    status_label_->setText("");
}

std::optional<AlertRecord> AlertPage::selectedAlert() const {
    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(alerts_.size())) {
        return std::nullopt;
    }
    return alerts_[row];
}

void AlertPage::setBusy(bool busy) {
    refresh_button_->setEnabled(!busy);
}

}  // namespace dsd
