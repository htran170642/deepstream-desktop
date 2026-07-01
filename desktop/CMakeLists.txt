find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

# Run moc automatically on any source/header that contains Q_OBJECT.
set(CMAKE_AUTOMOC ON)

# Qt-free gRPC clients. Separate library so the integration tests can link them
# without pulling in Qt or needing a QApplication.
add_library(desktop_client_lib STATIC
  src/client/GrpcSupport.hpp
  src/client/HealthClient.cpp
  src/client/HealthClient.hpp
  src/client/CameraClient.cpp
  src/client/CameraClient.hpp
  src/client/StreamClient.cpp
  src/client/StreamClient.hpp
  src/client/AlertClient.cpp
  src/client/AlertClient.hpp
  src/client/NotificationClient.cpp
  src/client/NotificationClient.hpp
  src/client/SystemClient.cpp
  src/client/SystemClient.hpp
)
target_include_directories(desktop_client_lib PUBLIC src)
target_link_libraries(desktop_client_lib PUBLIC dsd_proto dsd_logging)

# UI library: MainWindow + pages. Links the client lib and Qt.
add_library(desktop_lib STATIC
  src/MainWindow.cpp
  src/MainWindow.hpp
  src/pages/PlaceholderPage.cpp
  src/pages/PlaceholderPage.hpp
  src/pages/DashboardPage.cpp
  src/pages/DashboardPage.hpp
  src/pages/CameraPage.cpp
  src/pages/CameraPage.hpp
  src/pages/CameraDialog.cpp
  src/pages/CameraDialog.hpp
  src/pages/LiveViewPage.cpp
  src/pages/LiveViewPage.hpp
  src/pages/DetectionView.cpp
  src/pages/DetectionView.hpp
  src/pages/AlertPage.cpp
  src/pages/AlertPage.hpp
  src/pages/SettingsPage.cpp
  src/pages/SettingsPage.hpp
)
target_include_directories(desktop_lib PUBLIC src)
target_link_libraries(desktop_lib PUBLIC
  Qt6::Widgets
  Qt6::Concurrent
  desktop_client_lib
  dsd_logging
)

# Thin executable: just the entry point, linking the UI library.
add_executable(desktop
  src/main.cpp
)
target_link_libraries(desktop PRIVATE desktop_lib)
