#include "pages/CameraPage.hpp"

#include <QAbstractItemView>
#include <QFuture>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QLabel>

#include "pages/CameraDialog.hpp"

namespace dsd {

CameraPage::CameraPage(QWidget* parent)
    : QWidget(parent), client_(std::make_shared<CameraClient>()) {
    auto* layout = new QVBoxLayout(this);

    // Toolbar row of actions.
    auto* buttons = new QHBoxLayout();
    add_button_ = new QPushButton("Add", this);
    edit_button_ = new QPushButton("Edit", this);
    delete_button_ = new QPushButton("Delete", this);
    refresh_button_ = new QPushButton("Refresh", this);
    buttons->addWidget(add_button_);
    buttons->addWidget(edit_button_);
    buttons->addWidget(delete_button_);
    buttons->addWidget(refresh_button_);
    buttons->addStretch();
    layout->addLayout(buttons);

    // Read-only, single-row-selection table.
    table_ = new QTableWidget(this);
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"Name", "RTSP URL", "Enabled"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(table_);

    status_label_ = new QLabel(this);
    status_label_->setStyleSheet("color: gray;");
    layout->addWidget(status_label_);

    connect(add_button_, &QPushButton::clicked, this, &CameraPage::addCamera);
    connect(edit_button_, &QPushButton::clicked, this, &CameraPage::editCamera);
    connect(delete_button_, &QPushButton::clicked, this, &CameraPage::deleteCamera);
    connect(refresh_button_, &QPushButton::clicked, this, &CameraPage::refresh);

    connect(&list_watcher_,
            &QFutureWatcher<std::optional<std::vector<CameraInfo>>>::finished,
            this, &CameraPage::onListed);
    connect(&mutation_watcher_, &QFutureWatcher<bool>::finished,
            this, &CameraPage::onMutated);

    refresh();  // load the initial list
}

void CameraPage::refresh() {
    setBusy(true);
    auto client = client_;  // shared_ptr copy: worker outlives the page safely
    list_watcher_.setFuture(QtConcurrent::run([client] { return client->list(); }));
}

void CameraPage::onListed() {
    const std::optional<std::vector<CameraInfo>> result = list_watcher_.result();
    setBusy(false);

    if (!result) {
        // Non-modal: the desktop may legitimately run before the service is up.
        status_label_->setText("Service unavailable — could not load cameras.");
        return;
    }

    // Remember the selected camera (by id) so we can restore it after rebuild.
    std::optional<std::int64_t> selected_id;
    if (const std::optional<CameraInfo> current = selectedCamera()) {
        selected_id = current->id;
    }

    cameras_ = *result;
    table_->setRowCount(static_cast<int>(cameras_.size()));
    for (int row = 0; row < static_cast<int>(cameras_.size()); ++row) {
        const CameraInfo& c = cameras_[row];
        table_->setItem(row, 0,
                        new QTableWidgetItem(QString::fromStdString(c.name)));
        table_->setItem(row, 1,
                        new QTableWidgetItem(QString::fromStdString(c.rtspUrl)));
        table_->setItem(row, 2, new QTableWidgetItem(c.enabled ? "Yes" : "No"));

        if (selected_id && c.id == *selected_id) {
            table_->selectRow(row);  // restore previous selection
        }
    }

    status_label_->setText(cameras_.empty() ? "No cameras yet." : "");
}

void CameraPage::addCamera() {
    CameraDialog dialog(CameraInfo{}, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const CameraInfo info = dialog.camera();
    auto client = client_;
    runMutation([client, info] { return client->add(info).has_value(); });
}

void CameraPage::editCamera() {
    const std::optional<CameraInfo> selected = selectedCamera();
    if (!selected) {
        QMessageBox::information(this, "No selection", "Select a camera to edit.");
        return;
    }
    CameraDialog dialog(*selected, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const CameraInfo info = dialog.camera();
    auto client = client_;
    runMutation([client, info] { return client->update(info).has_value(); });
}

void CameraPage::deleteCamera() {
    const std::optional<CameraInfo> selected = selectedCamera();
    if (!selected) {
        QMessageBox::information(this, "No selection", "Select a camera to delete.");
        return;
    }
    const auto reply = QMessageBox::question(
        this, "Delete camera",
        QString("Delete camera \"%1\"?")
            .arg(QString::fromStdString(selected->name)));
    if (reply != QMessageBox::Yes) {
        return;
    }
    const std::int64_t id = selected->id;
    auto client = client_;
    runMutation([client, id] { return client->remove(id); });
}

void CameraPage::runMutation(std::function<bool()> op) {
    setBusy(true);
    mutation_watcher_.setFuture(QtConcurrent::run(std::move(op)));
}

void CameraPage::onMutated() {
    if (!mutation_watcher_.result()) {
        QMessageBox::warning(this, "Operation failed",
                             "The service rejected the operation.");
    }
    refresh();  // re-sync the table (also clears the busy state)
}

std::optional<CameraInfo> CameraPage::selectedCamera() const {
    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(cameras_.size())) {
        return std::nullopt;
    }
    return cameras_[row];
}

void CameraPage::setBusy(bool busy) {
    add_button_->setEnabled(!busy);
    edit_button_->setEnabled(!busy);
    delete_button_->setEnabled(!busy);
    refresh_button_->setEnabled(!busy);
}

}  // namespace dsd
