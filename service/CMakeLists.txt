# Service library: server impl + lifecycle + camera module + pipeline, so tests
# can link it without main().
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
  src/CameraServiceImpl.cpp
  src/CameraServiceImpl.hpp
  src/StreamServiceImpl.cpp
  src/StreamServiceImpl.hpp
  src/AlertRepository.cpp
  src/AlertRepository.hpp
  src/AlertManager.cpp
  src/AlertManager.hpp
  src/AlertServiceImpl.cpp
  src/AlertServiceImpl.hpp
  src/notify/NotificationChannel.hpp
  src/notify/Payloads.cpp
  src/notify/Payloads.hpp
  src/notify/NotificationManager.cpp
  src/notify/NotificationManager.hpp
  src/notify/ChannelFactory.cpp
  src/notify/ChannelFactory.hpp
  src/pipeline/Detection.hpp
  src/pipeline/Alert.hpp
  src/pipeline/Pipeline.hpp
  src/pipeline/DetectionProcessor.cpp
  src/pipeline/DetectionProcessor.hpp
  src/pipeline/FakePipeline.cpp
  src/pipeline/FakePipeline.hpp
  src/pipeline/PipelineFactory.cpp
  src/pipeline/PipelineFactory.hpp
  src/pipeline/PipelineManager.cpp
  src/pipeline/PipelineManager.hpp
)
target_include_directories(service_lib PUBLIC src)
target_link_libraries(service_lib PUBLIC dsd_proto dsd_logging)

# std::thread (FakePipeline / PipelineManager) needs the threads library.
find_package(Threads REQUIRED)
target_link_libraries(service_lib PUBLIC Threads::Threads)

# Notification transport (HTTP/SMTP) is optional, like DeepStream. When libcurl
# is present, compile the real channels and define DSD_WITH_CURL; otherwise
# ChannelFactory returns an empty list and the service runs without them.
find_package(CURL)
if(CURL_FOUND)
  target_sources(service_lib PRIVATE
    src/notify/Http.cpp
    src/notify/Http.hpp
    src/notify/SlackChannel.cpp
    src/notify/SlackChannel.hpp
    src/notify/WebhookChannel.cpp
    src/notify/WebhookChannel.hpp
    src/notify/TelegramChannel.cpp
    src/notify/TelegramChannel.hpp
    src/notify/EmailChannel.cpp
    src/notify/EmailChannel.hpp
  )
  target_compile_definitions(service_lib PRIVATE DSD_WITH_CURL=1)
  target_link_libraries(service_lib PRIVATE CURL::libcurl)
  message(STATUS "libcurl found — notification channels ENABLED")
else()
  message(STATUS "libcurl not found — notifications DISABLED (host build)")
endif()

# SQLite is an implementation detail of the repository (no sqlite types in
# public headers), so link it PRIVATE.
find_package(SQLite3 REQUIRED)
target_include_directories(service_lib PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(service_lib PRIVATE ${SQLite3_LIBRARIES})

# Thin executable: just the entry point, linking the service library.
add_executable(service
  src/main.cpp
)
target_link_libraries(service PRIVATE service_lib)

# DeepStream is optional on the host. Enabled inside the Docker container
# (NVIDIA DeepStream base image), where the SDK lives at the path below.
# Stage 5b will add DeepStreamPipeline.cpp + GStreamer/nvds inside this block.
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

# DeepStream is optional on the host. Enabled inside the Docker container
# (NVIDIA DeepStream base image), where the SDK lives at the path below.
if(ENABLE_DEEPSTREAM)
  set(DEEPSTREAM_ROOT "/opt/nvidia/deepstream/deepstream"
      CACHE PATH "DeepStream SDK root")
  message(STATUS "DeepStream ENABLED, SDK at ${DEEPSTREAM_ROOT}")

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0)

  # Compile the real pipeline into service_lib and enable the factory branch.
  target_sources(service_lib PRIVATE
    src/pipeline/DeepStreamPipeline.cpp
    src/pipeline/DeepStreamPipeline.hpp
  )
  target_compile_definitions(service_lib PRIVATE DSD_WITH_DEEPSTREAM=1)
  target_include_directories(service_lib PRIVATE
    ${DEEPSTREAM_ROOT}/sources/includes)
  target_link_directories(service_lib PUBLIC ${DEEPSTREAM_ROOT}/lib)
  target_link_libraries(service_lib PUBLIC
    PkgConfig::GST
    nvdsgst_meta
    nvds_meta
    nvds_batch_jpegenc
    cuda
  )
  # libnvbufsurface references CUDA driver symbols (cuCtxGetCurrent, ...) that
  # are provided at runtime by the injected NVIDIA driver. Allow them to be
  # undefined at build time (no GPU during docker build).
  target_link_options(service PRIVATE -Wl,--allow-shlib-undefined)


  # main.cpp uses DSD_WITH_DEEPSTREAM for the bind address + startup log.
  target_compile_definitions(service PRIVATE DSD_WITH_DEEPSTREAM=1)
else()
  message(STATUS "DeepStream disabled (ENABLE_DEEPSTREAM=OFF) — host build")
endif()
