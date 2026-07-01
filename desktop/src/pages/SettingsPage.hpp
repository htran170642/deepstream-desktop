#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QWidget>

#include "client/NotificationClient.hpp"

class QLabel;
class QPushButton;
class QTableWidget;

template <typename T>
class QFutureWatcher;

namespace dsd {

// Settings page. Currently hosts the Notifications section: a read-only view of
// which notification channels are configured on the service, plus a "send test"
// button. Channel config (secrets) lives server-side; this only shows names +
// enabled state. All gRPC work runs off the UI thread via QtConcurrent.
class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);
    ~SettingsPage() override;

private:
    void refreshStatus();     // run GetStatus
    void onStatusFetched();   // status_watcher_ finished
    void onSendTest();        // run SendTest
    void onTestFinished();    // test_watcher_ finished
    void setBusy(bool busy);

    std::shared_ptr<NotificationClient> client_;

    QTableWidget* table_ = nullptr;         // channel | status
    QPushButton* refresh_button_ = nullptr;
    QPushButton* test_button_ = nullptr;
    QLabel* status_label_ = nullptr;

    std::unique_ptr<
        QFutureWatcher<std::optional<std::vector<ChannelStatusRecord>>>>
        status_watcher_;
    std::unique_ptr<QFutureWatcher<std::optional<std::vector<DeliveryRecord>>>>
        test_watcher_;
};

}  // namespace dsd
