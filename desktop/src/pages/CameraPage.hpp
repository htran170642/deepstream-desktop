#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <QFutureWatcher>
#include <QWidget>

#include "client/CameraClient.hpp"

class QLabel;
class QPushButton;
class QTableWidget;

namespace dsd {

// Camera management page: lists cameras in a table and supports Add / Edit /
// Delete / Refresh. All gRPC calls run off the UI thread (QtConcurrent +
// QFutureWatcher); the page never touches gRPC beyond the CameraClient.
class CameraPage : public QWidget {
    Q_OBJECT

public:
    explicit CameraPage(QWidget* parent = nullptr);

private:
    void refresh();        // start ListCameras off-thread
    void onListed();       // populate the table from the list result
    void onMutated();      // refresh after an add/update/delete completes

    void addCamera();
    void editCamera();
    void deleteCamera();

    // Runs a client operation off the UI thread, then onMutated() on the UI thread.
    void runMutation(std::function<bool()> op);

    std::optional<CameraInfo> selectedCamera() const;
    void setBusy(bool busy);

    std::shared_ptr<CameraClient> client_;

    QTableWidget* table_ = nullptr;
    QLabel* status_label_ = nullptr;
    QPushButton* add_button_ = nullptr;
    QPushButton* edit_button_ = nullptr;
    QPushButton* delete_button_ = nullptr;
    QPushButton* refresh_button_ = nullptr;

    QFutureWatcher<std::optional<std::vector<CameraInfo>>> list_watcher_;
    QFutureWatcher<bool> mutation_watcher_;

    std::vector<CameraInfo> cameras_;  // current rows, parallel to the table
};

}  // namespace dsd
