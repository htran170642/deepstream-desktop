#pragma once

#include <memory>
#include <optional>

#include <QWidget>

#include "client/SystemClient.hpp"

class QLabel;
class QProgressBar;
class QTableWidget;
class QTimer;

template <typename T>
class QFutureWatcher;

namespace dsd {

// Dashboard page: polls SystemService::GetStats on a timer and shows CPU/mem/GPU
// usage, FPS, pipeline state, and per-camera status. gRPC runs off the UI thread
// (QtConcurrent + QFutureWatcher); a busy-guard skips a tick if one is in flight.
class DashboardPage : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget* parent = nullptr);
    ~DashboardPage() override;

private:
    void poll();              // kick off a GetStats unless one is in flight
    void onStatsFetched();    // watcher_ finished

    std::shared_ptr<SystemClient> client_;
    QTimer* timer_ = nullptr;
    bool busy_ = false;

    QProgressBar* cpu_bar_ = nullptr;
    QProgressBar* mem_bar_ = nullptr;
    QProgressBar* gpu_bar_ = nullptr;
    QLabel* fps_label_ = nullptr;
    QLabel* pipeline_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QTableWidget* camera_table_ = nullptr;

    std::unique_ptr<QFutureWatcher<std::optional<SystemStatsRecord>>> watcher_;
};

}  // namespace dsd
