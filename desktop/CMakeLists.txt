find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Run moc automatically on any source/header that contains Q_OBJECT.
set(CMAKE_AUTOMOC ON)

# UI library: holds MainWindow + pages so tests can link the UI without main().
add_library(desktop_lib STATIC
  src/MainWindow.cpp
  src/MainWindow.hpp
  src/pages/PlaceholderPage.cpp
  src/pages/PlaceholderPage.hpp
  src/pages/DashboardPage.cpp
  src/pages/DashboardPage.hpp
  src/pages/CameraPage.cpp
  src/pages/CameraPage.hpp
  src/pages/LiveViewPage.cpp
  src/pages/LiveViewPage.hpp
  src/pages/AlertPage.cpp
  src/pages/AlertPage.hpp
  src/pages/SettingsPage.cpp
  src/pages/SettingsPage.hpp
)

# `src` is exposed PUBLIC so both the exe and the tests can include
# "MainWindow.hpp" and "pages/...". dsd_logging brings in "logging/Logger.hpp".
target_include_directories(desktop_lib PUBLIC src)
target_link_libraries(desktop_lib PUBLIC Qt6::Widgets dsd_logging)

# Thin executable: just the entry point, linking the UI library.
add_executable(desktop
  src/main.cpp
)
target_link_libraries(desktop PRIVATE desktop_lib)
