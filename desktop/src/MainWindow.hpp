#pragma once
#include <memory>

#include <QFutureWatcher>
#include <QMainWindow>

#include "client/HealthClient.hpp"

// Forward declarations: we only use pointers here, so no need to include the
// full Qt headers in this header (faster compiles, less coupling).
class QAction;
class QListWidget;
class QStackedWidget;

namespace dsd {

// Page order must stay in sync with the sidebar rows and the stacked-widget
// page indices. The enum is the single source of truth for that mapping.
enum class Page {
    Dashboard = 0,
    Cameras,
    LiveView,
    Alerts,
    Settings,
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Programmatic navigation (also used by the navigation test).
    void selectPage(Page page);
    Page currentPage() const;

private:
    void buildToolBar();

    // Runs HealthClient::check() off the UI thread and updates the status bar.
    void checkService();
    void onHealthChecked();

    QListWidget* sidebar_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QAction* checkAction_ = nullptr;

    std::shared_ptr<HealthClient> health_client_;
    QFutureWatcher<ServiceHealth> health_watcher_;
};

}  // namespace dsd
