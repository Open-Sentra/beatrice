# Beatrice SDK

[![CI/CD Pipeline](https://github.com/your-org/beatrice/workflows/CI%2FCD%20Pipeline/badge.svg)](https://github.com/your-org/beatrice/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)](https://cmake.org/)

**Beatrice** is a zero-copy, low-level, high-performance network driver SDK written in modern C++ for capturing and analyzing network traffic with minimal latency. It provides a modular architecture that supports multiple capture backends and a plugin system for extensible packet processing.

## 🚀 Features

- **Zero-copy DMA access** using AF_XDP or DPDK for maximum performance
- **Modular backend abstraction** supporting AF_PACKET, DPDK, and AF_XDP
- **Plugin system** for runtime loading of packet processing modules
- **High-performance packet processing** with configurable batch sizes
- **Hardware timestamping** support for precise timing measurements
- **Multi-threading** with CPU affinity and thread pinning
- **Comprehensive metrics** and monitoring capabilities
- **Configuration management** with JSON and environment variable support
- **Professional logging** with multiple output sinks and log rotation
- **Error handling** with modern Result types and exception safety
- **Cross-platform** support for Linux and macOS

## 📋 Requirements

### System Requirements
- **Linux**: Kernel 5.4+ (for AF_XDP support)
- **macOS**: 10.15+ (limited backend support)
- **CPU**: x86_64 or ARM64 architecture
- **Memory**: 4GB+ RAM recommended
- **Network**: Supported NIC with driver support

### Software Dependencies
- **CMake**: 3.20 or higher
- **Compiler**: GCC 9+ or Clang 10+ with C++20 support
- **Build Tools**: Make, pkg-config
- **Libraries**: libpcap-dev, libbpf-dev (Linux)

### Optional Dependencies
- **nlohmann-json**: For configuration management
- **spdlog**: For advanced logging capabilities
- **fmt**: For string formatting utilities

## 🛠️ Installation

### Quick Start

```bash
# Clone the repository
git clone https://github.com/your-org/beatrice.git
cd beatrice

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Install (optional)
sudo make install
```

### Advanced Build Options

```bash
# Debug build with sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..

# Release build with specific options
cmake -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=ON \
      -DBUILD_EXAMPLES=ON \
      -DBUILD_DOCS=ON ..

# Build with specific compiler
cmake -DCMAKE_CXX_COMPILER=clang++ ..
```

### Package Managers

#### Ubuntu/Debian
```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake libpcap-dev libbpf-dev

# Build and install
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

#### Fedora/RHEL
```bash
# Install dependencies
sudo dnf install gcc-c++ cmake libpcap-devel libbpf-devel

# Build and install
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

#### macOS
```bash
# Install dependencies
brew install cmake pkg-config

# Build and install
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
make install
```

## 📖 Usage

### Basic Example

```cpp
#include "beatrice/BeatriceContext.hpp"
#include "beatrice/AF_XDPBackend.hpp"
#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"

int main() {
    // Initialize logging
    beatrice::Logger::initialize("beatrice_example");
    
    // Create backend and plugin manager
    auto backend = std::make_unique<beatrice::AF_XDPBackend>();
    auto pluginMgr = std::make_unique<beatrice::PluginManager>();
    
    // Create context
    beatrice::BeatriceContext context(std::move(backend), std::move(pluginMgr));
    
    // Initialize and run
    if (context.initialize()) {
        BEATRICE_INFO("Beatrice context initialized successfully");
        context.run();
    } else {
        BEATRICE_ERROR("Failed to initialize Beatrice context");
        return 1;
    }
    
    return 0;
}
```

### Configuration Example

```cpp
#include "beatrice/Config.hpp"

// Initialize configuration
beatrice::Config::initialize("config.json");

// Get configuration values
auto& config = beatrice::Config::get();
std::string interface = config.getString("network.interface", "eth0");
size_t bufferSize = config.getInt("network.bufferSize", 4096);
bool promiscuous = config.getBool("network.promiscuous", true);
```

### Plugin Development

```cpp
#include "beatrice/IPacketPlugin.hpp"

class MyPacketPlugin : public beatrice::IPacketPlugin {
public:
    void onPacket(beatrice::Packet& packet) override {
        // Process packet
        if (packet.isTCP()) {
            BEATRICE_DEBUG("Processing TCP packet: {} bytes", packet.size());
            // Your packet processing logic here
        }
    }
};

// Export plugin
extern "C" beatrice::IPacketPlugin* createPlugin() {
    return new MyPacketPlugin();
}
```

## 🔧 Configuration

### Configuration File Format

```json
{
  "logging": {
    "level": "info",
    "file": "/var/log/beatrice.log",
    "maxFileSize": 10,
    "maxFiles": 5,
    "console": true
  },
  "network": {
    "interface": "eth0",
    "backend": "af_xdp",
    "bufferSize": 4096,
    "numBuffers": 1024,
    "promiscuous": true,
    "timeout": 1000
  },
  "plugins": {
    "directory": "./plugins",
    "enabled": ["tcp_analyzer", "http_parser"],
    "autoLoad": false,
    "maxPlugins": 10
  },
  "performance": {
    "numThreads": 4,
    "pinThreads": true,
    "cpuAffinity": [0, 1, 2, 3],
    "batchSize": 64,
    "enableMetrics": true
  }
}
```

### Environment Variables

```bash
# Logging
export BEATRICE_LOGGING_LEVEL=debug
export BEATRICE_LOGGING_FILE=/var/log/beatrice.log

# Network
export BEATRICE_NETWORK_INTERFACE=eth0
export BEATRICE_NETWORK_BACKEND=af_xdp

# Performance
export BEATRICE_PERFORMANCE_NUM_THREADS=8
export BEATRICE_PERFORMANCE_BATCH_SIZE=128
```

## 🧪 Testing

### Run Tests

```bash
# Build with tests
cmake -DBUILD_TESTS=ON ..
make -j$(nproc)

# Run all tests
make test

# Run specific test
./tests/beatrice_tests --gtest_filter=PacketTest.*

# Run with verbose output
ctest --output-on-failure --verbose
```

### Test Categories

- **Unit Tests**: Individual component testing
- **Integration Tests**: Component interaction testing
- **Performance Tests**: Performance benchmarking
- **Memory Tests**: Memory leak detection with Valgrind

### Coverage

```bash
# Enable coverage
cmake -DENABLE_COVERAGE=ON ..
make -j$(nproc)

# Generate coverage report
make coverage

# View coverage report
open coverage_report/index.html
```

## 📊 Performance

### Benchmarks

| Backend | Packets/sec | Latency (μs) | CPU Usage |
|---------|-------------|---------------|-----------|
| AF_XDP  | 10M+        | <1           | 15%       |
| DPDK    | 15M+        | <0.5         | 10%       |
| AF_PACKET| 2M          | <5           | 40%       |

### Optimization Tips

1. **Use appropriate batch sizes** for your workload
2. **Enable CPU affinity** for consistent performance
3. **Use zero-copy backends** when possible
4. **Monitor metrics** to identify bottlenecks
5. **Profile with tools** like perf, gprof, or Intel VTune

## 🔌 Plugin System

### Plugin Interface

```cpp
class IPacketPlugin {
public:
    virtual ~IPacketPlugin() = default;
    virtual void onPacket(Packet& packet) = 0;
    virtual void onStart() = 0;
    virtual void onStop() = 0;
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
};
```

### Plugin Lifecycle

1. **Discovery**: Plugin manager scans plugin directory
2. **Loading**: Dynamic library loading with symbol resolution
3. **Initialization**: Plugin setup and configuration
4. **Execution**: Packet processing in real-time
5. **Cleanup**: Resource cleanup and library unloading

### Available Plugins

- **TCP Analyzer**: TCP connection tracking and analysis
- **HTTP Parser**: HTTP request/response parsing
- **DNS Monitor**: DNS query monitoring
- **Security Scanner**: Malicious traffic detection
- **Statistics Collector**: Packet statistics aggregation

## 📈 Monitoring & Metrics

### Built-in Metrics

- **Packet counters**: Captured, dropped, processed
- **Performance metrics**: Latency, throughput, CPU usage
- **System metrics**: Memory usage, buffer utilization
- **Custom metrics**: User-defined application metrics

### Metrics Export

```bash
# Prometheus format
curl http://localhost:8080/metrics

# JSON format
curl http://localhost:8080/metrics/json

# Health check
curl http://localhost:8080/health
```

### Integration

- **Prometheus**: Metrics collection and alerting
- **Grafana**: Visualization and dashboards
- **Jaeger**: Distributed tracing
- **ELK Stack**: Log aggregation and analysis

## 🐛 Troubleshooting

### Common Issues

#### Permission Denied
```bash
# Check capabilities
sudo setcap cap_net_raw,cap_net_admin=eip /usr/local/bin/beatrice

# Or run with sudo
sudo ./beatrice
```

#### Interface Not Found
```bash
# List available interfaces
ip link show

# Check interface status
ip link set eth0 up
```

#### Performance Issues
```bash
# Check CPU frequency
cat /proc/cpuinfo | grep MHz

# Check interrupt distribution
cat /proc/interrupts | grep eth0

# Monitor system resources
htop
```

### Debug Mode

```bash
# Enable debug logging
export BEATRICE_LOGGING_LEVEL=debug

# Run with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
gdb ./beatrice
```

### Log Analysis

```bash
# View real-time logs
tail -f /var/log/beatrice.log

# Search for errors
grep ERROR /var/log/beatrice.log

# Analyze log patterns
awk '/ERROR/ {print $4}' /var/log/beatrice.log | sort | uniq -c
```

## 🤝 Contributing

### Development Setup

```bash
# Clone repository
git clone https://github.com/your-org/beatrice.git
cd beatrice

# Create development branch
git checkout -b feature/your-feature-name

# Install development dependencies
sudo apt install clang-format cppcheck valgrind

# Setup pre-commit hooks
cp .git/hooks/pre-commit.sample .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

### Code Style

- Follow [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- Use `clang-format` for consistent formatting
- Run `cppcheck` for static analysis
- Ensure all tests pass before submitting

### Pull Request Process

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Update documentation
6. Submit a pull request

### Code Review Checklist

- [ ] Code follows style guidelines
- [ ] Tests are included and passing
- [ ] Documentation is updated
- [ ] No memory leaks or undefined behavior
- [ ] Performance impact is considered

## 📚 Documentation

### API Reference

- [API Documentation](docs/api/) - Generated with Doxygen
- [User Guide](docs/user_guide/) - Comprehensive usage examples
- [Architecture](docs/architecture.md) - System design and components
- [Plugin Development](docs/plugin_development.md) - Plugin creation guide

### Examples

- [Basic Usage](examples/basic_usage.cpp)
- [Plugin Development](examples/plugin_example.cpp)
- [Configuration](examples/configuration.cpp)
- [Performance Tuning](examples/performance.cpp)

### Tutorials

- [Getting Started](docs/tutorials/getting_started.md)
- [Building Plugins](docs/tutorials/building_plugins.md)
- [Performance Optimization](docs/tutorials/performance.md)
- [Deployment](docs/tutorials/deployment.md)

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **AF_XDP**: Linux kernel team for XDP sockets
- **DPDK**: Intel for Data Plane Development Kit
- **spdlog**: Gabi Melman for fast logging library
- **nlohmann/json**: Niels Lohmann for JSON library
- **Google Test**: Google for testing framework

## 📞 Support

### Getting Help

- **Documentation**: [docs/](docs/)
- **Issues**: [GitHub Issues](https://github.com/your-org/beatrice/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-org/beatrice/discussions)
- **Email**: support@your-org.com

### Community

- **Slack**: [Join our workspace](https://your-org.slack.com)
- **Discord**: [Join our server](https://discord.gg/your-org)
- **Mailing List**: [Subscribe here](https://groups.google.com/g/beatrice-dev)

### Commercial Support

For enterprise customers, we offer:
- **Professional support** with SLA guarantees
- **Custom development** and consulting
- **Training and workshops**
- **Performance optimization** services

Contact us at enterprise@your-org.com for more information.

---

**Made with ❤️ by the Beatrice Team**

[Website](https://beatrice.dev) • [Blog](https://blog.beatrice.dev) • [Twitter](https://twitter.com/beatrice_sdk)
