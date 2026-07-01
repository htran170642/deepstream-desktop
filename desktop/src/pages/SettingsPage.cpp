#include "pages/SettingsPage.hpp"

#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace dsd {

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent),
      client_(std::make_shared<NotificationClient>()) {
    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel("<h2>Settings</h2>", this));
    root->addWidget(new QLabel("<b>Notifications</b>", this));
    root->addWidget(new QLabel(
        "Channels are configured on the service via environment variables.",
        this));

    table_ = new QTableWidget(0, 2, this);
    table_->setHorizontalHeaderLabels({"Channel", "Status"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    root->addWidget(table_);

    auto* buttons = new QHBoxLayout();
    refresh_button_ = new QPushButton("Refresh", this);
    test_button_ = new QPushButton("Send test notification", this);
    buttons->addWidget(refresh_button_);
    buttons->addWidget(test_button_);
    buttons->addStretch();
    root->addLayout(buttons);

    status_label_ = new QLabel(this);
    status_label_->setWordWrap(true);
    root->addWidget(status_label_);

    status_watcher_ = std::make_unique<
        QFutureWatcher<std::optional<std::vector<ChannelStatusRecord>>>>();
    test_watcher_ = std::make_unique<
        QFutureWatcher<std::optional<std::vector<DeliveryRecord>>>>();
    connect(status_watcher_.get(),
            &QFutureWatcher<std::optional<std::vector<ChannelStatusRecord>>>::
                finished,
            this, &SettingsPage::onStatusFetched);
    connect(test_watcher_.get(),
            &QFutureWatcher<
                std::optional<std::vector<DeliveryRecord>>>::finished,
            this, &SettingsPage::onTestFinished);

    connect(refresh_button_, &QPushButton::clicked, this,
            &SettingsPage::refreshStatus);
    connect(test_button_, &QPushButton::clicked, this,
            &SettingsPage::onSendTest);

    refreshStatus();
}

SettingsPage::~SettingsPage() {
    // Let any in-flight watcher finish before its result vector is destroyed.
    if (status_watcher_) status_watcher_->waitForFinished();
    if (test_watcher_) test_watcher_->waitForFinished();
}

void SettingsPage::setBusy(bool busy) {
    refresh_button_->setEnabled(!busy);
    test_button_->setEnabled(!busy);
}

void SettingsPage::refreshStatus() {
    setBusy(true);
    status_label_->setText("Loading channel status…");
    auto client = client_;  // capture the shared_ptr, not this
    status_watcher_->setFuture(
        QtConcurrent::run([client] { return client->status(); }));
}

void SettingsPage::onStatusFetched() {
    setBusy(false);
    const auto result = status_watcher_->result();
    if (!result) {
        status_label_->setText("Could not reach the service.");
        return;
    }
    table_->setRowCount(0);
    for (const auto& ch : *result) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        table_->setItem(row, 0,
                        new QTableWidgetItem(QString::fromStdString(ch.name)));
        table_->setItem(
            row, 1,
            new QTableWidgetItem(ch.enabled ? "enabled" : "not configured"));
    }
    status_label_->setText(QString("%1 channel(s).").arg(result->size()));
}

void SettingsPage::onSendTest() {
    setBusy(true);
    status_label_->setText("Sending test notification…");
    auto client = client_;
    test_watcher_->setFuture(
        QtConcurrent::run([client] { return client->sendTest(); }));
}

void SettingsPage::onTestFinished() {
    setBusy(false);
    const auto result = test_watcher_->result();
    if (!result) {
        status_label_->setText("Test failed: could not reach the service.");
        return;
    }
    if (result->empty()) {
        status_label_->setText("No enabled channels to test.");
        return;
    }
    QStringList parts;
    for (const auto& r : *result) {
        parts << QString("%1: %2")
                     .arg(QString::fromStdString(r.name),
                          r.ok ? "ok" : "failed");
    }
    status_label_->setText("Test result — " + parts.join(", "));
}

}  // namespace dsd
