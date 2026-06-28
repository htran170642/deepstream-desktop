add_executable(service
  src/main.cpp
)
target_link_libraries(service PRIVATE dsd_logging dsd_proto)

find_package(SQLite3 REQUIRED)
target_link_libraries(service PRIVATE SQLite3::SQLite3)

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
