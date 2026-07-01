# Build and run DeepStreamService inside the DeepStream 8.0 container.
FROM nvcr.io/nvidia/deepstream:8.0-triton-multiarch

# Build dependencies. DeepStream + GStreamer runtime already ship in the base;
# we add the -dev headers, build tools, and our other libraries.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        libgstreamer1.0-dev \
        libgrpc++-dev \
        libprotobuf-dev \
        protobuf-compiler \
        protobuf-compiler-grpc \
        libsqlite3-dev \
        libspdlog-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace

# Service-only build: DeepStream ON, no Qt desktop, no tests.
RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_DEEPSTREAM=ON \
        -DBUILD_DESKTOP=OFF \
        -DBUILD_TESTING=OFF \
    && cmake --build build --target service

# nvinfer config is mounted at runtime; the TensorRT engine cache lives in a
# volume so it is built once.
ENV DSD_NVINFER_CONFIG=/configs/config_infer_primary.txt

# Entrypoint pre-builds + caches the TensorRT engine, then execs the service.
COPY deploy/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Override the base image's banner entrypoint with ours.
ENTRYPOINT ["/entrypoint.sh"]
