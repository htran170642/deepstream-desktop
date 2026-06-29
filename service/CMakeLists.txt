# Service library: server impl + lifecycle + camera module, so tests can link
# it without main().
add_library(service_lib STATIC
  src/HealthServiceImpl.cpp
  src/HealthServiceImpl.hpp
  src/GrpcServer.cpp
  src/GrpcServer.hpp
  src/CameraModel.hpp
  src/CameraRepository.cpp
  src/CameraRepository.hpp
  src/CameraManager.cpp
  src/CameraManager.hpp
  src/CameraServiceImpl.cpp
  src/CameraServiceImpl.hpp
)
target_include_directories(service_lib PUBLIC src)
target_link_libraries(service_lib PUBLIC dsd_proto dsd_logging)

# SQLite is an implementation detail of the repository (no sqlite types in
# public headers), so link it PRIVATE.
find_package(SQLite3 REQUIRED)
target_link_libraries(service_lib PRIVATE SQLite3::SQLite3)

# Thin executable: just the entry point, linking the service library.
add_executable(service
  src/main.cpp
)
target_link_libraries(service PRIVATE service_lib)

# DeepStream is optional on the host. Enabled inside the Docker container
# (NVIDIA DeepStream base image), where the SDK lives at the path below.
if(ENABLE_DEEPSTREAM)
  set(DEEPSTREAM_ROOT "/opt/nvidia/deepstream/deepstream"
      CACHE PATH "DeepStream SDK root")
  message(STATUS "DeepStream ENABLED, SDK at ${DEEPSTREAM_ROOT}")
  target_include_directories(service PRIVATE ${DEEPSTREAM_ROOT}/sources/includes)
  target_link_directories(service PRIVATE ${DEEPSTREAM_ROOT}/lib)
  target_link_libraries(service PRIVATE nvdsgst_meta nvds_meta)
  target_compile_definitions(service PRIVATE DSD_WITH_DEEPSTREAM=1)
else()
  message(STATUS "DeepStream disabled (ENABLE_DEEPSTREAM=OFF) — host build")
endif()
