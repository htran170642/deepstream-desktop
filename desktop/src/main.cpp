#include <QApplication>

#include "MainWindow.hpp"
#include "logging/Logger.hpp"

namespace {

// Light, consistent app-wide styling. Kept inline (no Qt resource file) since
// it's small and there's a single theme. The sidebar is targeted by objectName.
constexpr char kStyleSheet[] = R"qss(
QWidget { font-size: 13px; }

QListWidget#sidebar {
    background: #2c3e50;
    color: #ecf0f1;
    border: none;
    outline: 0;
}
QListWidget#sidebar::item { padding: 12px 16px; }
QListWidget#sidebar::item:selected { background: #34495e; }
QListWidget#sidebar::item:hover { background: #3d566e; }

QPushButton {
    padding: 6px 14px;
    border: 1px solid #bdc3c7;
    border-radius: 4px;
    background: #ecf0f1;
}
QPushButton:hover { background: #dfe6e9; }
QPushButton:disabled { color: #95a5a6; }

QHeaderView::section {
    background: #ecf0f1;
    padding: 6px;
    border: none;
    border-bottom: 1px solid #bdc3c7;
    font-weight: bold;
}
QTableWidget { gridline-color: #ecf0f1; }

QProgressBar {
    border: 1px solid #bdc3c7;
    border-radius: 4px;
    text-align: center;
    min-height: 20px;
}
QProgressBar::chunk { background: #3498db; border-radius: 3px; }
)qss";

}  // namespace

int main(int argc, char* argv[]) {
    dsd::Logger::init("desktop");
    dsd::Logger::get()->info("Starting DeepStreamDesktop");

    QApplication app(argc, argv);
    QApplication::setApplicationName("DeepStreamDesktop");
    QApplication::setApplicationDisplayName("DeepStreamDesktop");
    QApplication::setOrganizationName("DeepStream");
    app.setStyleSheet(kStyleSheet);

    dsd::MainWindow window;
    window.show();

    return app.exec();
}
