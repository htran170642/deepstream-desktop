#include "MainWindow.hpp"

#include <QAction>
#include <QFuture>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QtConcurrent>

#include "logging/Logger.hpp"
#include "pages/AlertPage.hpp"
#include "pages/CameraPage.hpp"
#include "pages/DashboardPage.hpp"
#include "pages/LiveViewPage.hpp"
#include "pages/SettingsPage.hpp"

namespace dsd {

MainWindow::MainWindow(QWidget* parent) : 
    QMainWindow(parent),
    health_client_(std::make_shared<HealthClient>())
{
    setWindowTitle("DeepStreamDesktop");
    resize(1024, 768);

    // Central widget: sidebar on the left, page stack filling the rest.
    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    sidebar_ = new QListWidget(central);
    sidebar_->setFixedWidth(180);
    sidebar_->addItem("Dashboard");
    sidebar_->addItem("Cameras");
    sidebar_->addItem("Live View");
    sidebar_->addItem("Alerts");
    sidebar_->addItem("Settings");

    // Pages are added in Page enum order so row index == page index.
    stack_ = new QStackedWidget(central);
    stack_->addWidget(new DashboardPage(this));
    stack_->addWidget(new CameraPage(this));
    stack_->addWidget(new LiveViewPage(this));
    stack_->addWidget(new AlertPage(this));
    stack_->addWidget(new SettingsPage(this));

    layout->addWidget(sidebar_);
    layout->addWidget(stack_, 1);  // stretch factor 1: stack takes remaining space
    setCentralWidget(central);

    // Selecting a sidebar row switches the visible page.
    connect(sidebar_, &QListWidget::currentRowChanged,
            stack_, &QStackedWidget::setCurrentIndex);

    // The worker thread signals back here when the health check finishes.
    connect(&health_watcher_, &QFutureWatcher<ServiceHealth>::finished,
            this, &MainWindow::onHealthChecked);

    buildToolBar();
    statusBar()->showMessage("Ready");

    selectPage(Page::Dashboard);  // start on the first page
    Logger::get()->info("MainWindow constructed");
}

void MainWindow::buildToolBar() {
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    checkAction_ = toolbar->addAction("Check Service");
    connect(checkAction_, &QAction::triggered, this, &MainWindow::checkService);
}

void MainWindow::checkService() {
    // Disable the action while a check is in flight to avoid overlapping calls.
    checkAction_->setEnabled(false);
    statusBar()->showMessage("Checking service…");

    // Capture a shared_ptr copy (not `this`): the worker keeps the client alive
    // even if MainWindow is destroyed mid-check, avoiding a use-after-free.
    auto client = health_client_;
    QFuture<ServiceHealth> future =
        QtConcurrent::run([client] { return client->check(); });
    health_watcher_.setFuture(future);

}

void MainWindow::onHealthChecked() {
    QString message;
    switch (health_watcher_.result()) {
        case ServiceHealth::Serving:
            message = "Service: SERVING";
            break;
        case ServiceHealth::NotServing:
            message = "Service: NOT SERVING";
            break;
        case ServiceHealth::Unreachable:
            message = "Service: unreachable";
            break;
    }

    statusBar()->showMessage(message);
    checkAction_->setEnabled(true);
}

void MainWindow::selectPage(Page page) {
    sidebar_->setCurrentRow(static_cast<int>(page));
}

Page MainWindow::currentPage() const {
    return static_cast<Page>(stack_->currentIndex());
}

}  // namespace dsd
