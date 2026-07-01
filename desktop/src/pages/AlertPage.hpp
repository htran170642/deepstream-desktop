#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <QWidget>

#include "client/AlertClient.hpp"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

template <typename T>
class QFutureWatcher;

namespace dsd {

// Alert history page: filter/search past alerts, view a selected alert's JPEG
// snapshot, and optionally live-stream new alerts as they fire. All gRPC work
// runs off the UI thread (QtConcurrent for unary calls; a worker thread inside
// AlertClient for the stream).
class AlertPage : public QWidget {
    Q_OBJECT

public:
    explicit AlertPage(QWidget* parent = nullptr);
    ~AlertPage() override;

private:
    void refresh();                       // run ListAlerts with current filters
    void onListed();                      // list_watcher_ finished
    void onSelectionChanged();            // fetch snapshot for the selected alert
    void onSnapshotFetched();             // snapshot_watcher_ finished
    void toggleLive(bool on);             // subscribe/unsubscribe the stream
    void onLiveAlert(std::shared_ptr<const AlertRecord> alert);  // UI thread

    void populateTable();                 // rebuild rows from alerts_
    std::optional<AlertRecord> selectedAlert() const;
    void setBusy(bool busy);

    std::shared_ptr<AlertClient> client_;

    // Filter controls.
    QSpinBox* camera_id_spin_ = nullptr;  // 0 = any camera
    QLineEdit* label_edit_ = nullptr;     // substring search
    QSpinBox* limit_spin_ = nullptr;
    QPushButton* refresh_button_ = nullptr;
    QCheckBox* live_check_ = nullptr;

    QTableWidget* table_ = nullptr;
    QLabel* snapshot_label_ = nullptr;    // JPEG preview of the selected alert
    QLabel* status_label_ = nullptr;

    std::vector<AlertRecord> alerts_;     // rows currently shown (newest first)
    std::int64_t snapshot_for_id_ = -1;   // alert id the pending fetch belongs to

    std::unique_ptr<QFutureWatcher<std::optional<std::vector<AlertRecord>>>>
        list_watcher_;
    std::unique_ptr<QFutureWatcher<std::optional<std::vector<std::uint8_t>>>>
        snapshot_watcher_;
};

}  // namespace dsd
