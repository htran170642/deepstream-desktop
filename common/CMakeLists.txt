# ---------------------------------------------------------------------------
# dsd_logging : spdlog wrapper, shared by desktop and service.
# ---------------------------------------------------------------------------
find_package(spdlog REQUIRED)

add_library(dsd_logging
  logging/Logger.cpp
)
target_include_directories(dsd_logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dsd_logging PUBLIC spdlog::spdlog)

# ---------------------------------------------------------------------------
# dsd_proto : protobuf + gRPC stubs generated from proto/health.proto.
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf)
pkg_check_modules(GRPCPP   REQUIRED IMPORTED_TARGET grpc++)

find_program(PROTOC_EXECUTABLE protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_FILE ${PROTO_SRC_DIR}/health.proto)

set(PROTO_GEN
  ${PROTO_GEN_DIR}/health.pb.cc
  ${PROTO_GEN_DIR}/health.pb.h
  ${PROTO_GEN_DIR}/health.grpc.pb.cc
  ${PROTO_GEN_DIR}/health.grpc.pb.h
)

add_custom_command(
  OUTPUT ${PROTO_GEN}
  COMMAND ${PROTOC_EXECUTABLE}
    --proto_path=${PROTO_SRC_DIR}
    --cpp_out=${PROTO_GEN_DIR}
    --grpc_out=${PROTO_GEN_DIR}
    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
    ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  COMMENT "Generating C++ sources from health.proto"
  VERBATIM
)

add_library(dsd_proto
  ${PROTO_GEN}
)
target_include_directories(dsd_proto PUBLIC ${PROTO_GEN_DIR})
target_link_libraries(dsd_proto PUBLIC PkgConfig::GRPCPP PkgConfig::PROTOBUF)
