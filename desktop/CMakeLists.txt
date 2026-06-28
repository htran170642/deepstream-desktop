find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

add_executable(desktop
  src/main.cpp
)
target_link_libraries(desktop PRIVATE Qt6::Widgets dsd_logging)
