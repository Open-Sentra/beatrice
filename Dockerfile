# Multi-stage build for Beatrice Network Packet Processing SDK
FROM ubuntu:22.04 as base

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC
ENV BEATRICE_VERSION=1.0.0

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libbpf-dev \
    libelf-dev \
    zlib1g-dev \
    libssl-dev \
    libpcap-dev \
    libnuma-dev \
    python3 \
    python3-pip \
    curl \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Install DPDK dependencies
RUN apt-get update && apt-get install -y \
    meson \
    ninja-build \
    libnuma-dev \
    libpcap-dev \
    libssl-dev \
    libcrypto++-dev \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Install DPDK
RUN cd /tmp && \
    wget https://fast.dpdk.org/rel/dpdk-24.11.1.tar.xz && \
    tar xf dpdk-24.11.1.tar.xz && \
    cd dpdk-24.11.1 && \
    meson build && \
    ninja -C build && \
    ninja -C build install && \
    ldconfig && \
    rm -rf /tmp/dpdk-24.11.1*

# Install spdlog and nlohmann-json
RUN apt-get update && apt-get install -y \
    libspdlog-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Install fmt library
RUN apt-get update && apt-get install -y \
    libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /opt/beatrice

# Copy source code
COPY . .

# Build Beatrice
RUN mkdir -p build && \
    cd build && \
    cmake -DENABLE_DPDK=ON -DBUILD_EXAMPLES=ON -DBUILD_TESTS=ON .. && \
    make -j$(nproc) && \
    make install

# Runtime stage
FROM ubuntu:22.04 as runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libbpf0 \
    libelf1 \
    libpcap0.8 \
    libnuma1 \
    libssl3 \
    libcrypto++8 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libspdlog1 \
    libfmt8 \
    && rm -rf /var/lib/apt/lists/*

# Copy DPDK runtime libraries
COPY --from=base /usr/local/lib/x86_64-linux-gnu /usr/local/lib/x86_64-linux-gnu
COPY --from=base /usr/local/lib /usr/local/lib

# Copy Beatrice binaries and libraries
COPY --from=base /usr/local/bin/beatrice /usr/local/bin/
COPY --from=base /usr/local/bin/beatrice_cli /usr/local/bin/
COPY --from=base /usr/local/lib/libbeatrice_core.so /usr/local/lib/

# Copy examples
COPY --from=base /usr/local/bin/af_packet_example /usr/local/bin/
COPY --from=base /usr/local/bin/zero_copy_dma_test /usr/local/bin/

# Set up runtime environment
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
ENV PATH=/usr/local/bin:$PATH

# Create non-root user
RUN useradd -m -s /bin/bash beatrice && \
    chown -R beatrice:beatrice /opt/beatrice

# Switch to non-root user
USER beatrice

# Set working directory
WORKDIR /opt/beatrice

# Expose ports (if needed for network testing)
EXPOSE 8080

# Default command
CMD ["beatrice_cli", "--help"]

# Labels
LABEL version="${BEATRICE_VERSION}"
LABEL description="Beatrice Network Packet Processing SDK"
LABEL org.opencontainers.image.source="https://github.com/Open-Sentra/beatrice" 