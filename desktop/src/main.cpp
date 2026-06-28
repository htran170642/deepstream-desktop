#include <QApplication>
#include <QMainWindow>

#include "logging/Logger.hpp"

int main(int argc, char* argv[]) {
    dsd::Logger::init("desktop");
    dsd::Logger::get()->info("Starting DeepStreamDesktop");

    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("DeepStreamDesktop");
    window.resize(1024, 768);
    window.show();

    return app.exec();
}
