#include <QApplication>

#include "MainWindow.hpp"
#include "logging/Logger.hpp"

int main(int argc, char* argv[]) {
    dsd::Logger::init("desktop");
    dsd::Logger::get()->info("Starting DeepStreamDesktop");

    QApplication app(argc, argv);

    dsd::MainWindow window;
    window.show();

    return app.exec();
}
