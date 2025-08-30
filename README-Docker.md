# Beatrice Docker Guide

This guide explains how to use Beatrice with Docker for development, testing, and production deployment.

## Quick Start

### Build and Run

```bash
# Build the Docker image
docker build -t beatrice:latest .

# Run Beatrice CLI
docker run --rm -it --privileged --network host beatrice:latest

# Run with volume mounts for data persistence
docker run --rm -it --privileged --network host \
  -v $(pwd)/data:/opt/beatrice/data \
  -v $(pwd)/logs:/opt/beatrice/logs \
  beatrice:latest
```

### Using Docker Compose

```bash
# Start all services
docker-compose up -d

# Start specific service
docker-compose up -d beatrice

# View logs
docker-compose logs -f beatrice

# Stop all services
docker-compose down
```

## Services

### 1. Production Service (`beatrice`)

The main production service with optimized runtime.

```bash
# Start production service
docker-compose up -d beatrice

# Check status
docker-compose ps beatrice

# View logs
docker-compose logs -f beatrice
```

**Features:**
- Optimized runtime environment
- Health checks
- Resource limits
- Production-ready configuration

### 2. Development Service (`beatrice-dev`)

Interactive development environment with source code mounting.

```bash
# Start development environment
docker-compose up -d beatrice-dev

# Enter the container
docker-compose exec beatrice-dev bash

# Build from source
cd /opt/beatrice
mkdir -p build && cd build
cmake -DENABLE_DPDK=ON -DBUILD_EXAMPLES=ON ..
make -j$(nproc)
```

**Features:**
- Source code mounting
- Interactive shell
- Development tools
- Hot reload capability

### 3. Testing Service (`beatrice-test`)

Automated testing environment.

```bash
# Run tests
docker-compose up beatrice-test

# View test results
ls -la test-results/
cat test-results/results.csv
```

**Features:**
- Automated testing
- Test result collection
- CI/CD integration

### 4. Benchmark Service (`beatrice-benchmark`)

Performance benchmarking environment.

```bash
# Run benchmarks
docker-compose up beatrice-benchmark

# View benchmark results
ls -la benchmark-results/
cat benchmark-results/benchmark.csv
```

**Features:**
- Performance testing
- Benchmark result collection
- Performance analysis

## Configuration

### Environment Variables

```bash
# Production
BEATRICE_ENV=docker
BEATRICE_LOG_LEVEL=info
DPDK_EAL_ARGS=--lcores=0-3 --huge-dir=/dev/hugepages

# Development
BEATRICE_ENV=development
BEATRICE_LOG_LEVEL=debug
CMAKE_BUILD_TYPE=Debug

# Testing
BEATRICE_ENV=testing
BEATRICE_LOG_LEVEL=debug
BEATRICE_TEST_MODE=true
```

### Volume Mounts

```bash
# Data persistence
./data:/opt/beatrice/data

# Logs
./logs:/opt/beatrice/logs

# Configuration
./config:/opt/beatrice/config

# Test results
./test-results:/opt/beatrice/test-results

# Benchmark results
./benchmark-results:/opt/beatrice/benchmark-results
```

## Network Configuration

### Host Network Mode

For high-performance packet processing, Beatrice uses host network mode:

```bash
docker run --rm -it --privileged --network host beatrice:latest
```

**Benefits:**
- Direct network interface access
- Maximum performance
- No network overhead

**Requirements:**
- `--privileged` flag for DPDK access
- Host network mode for interface access

### Bridge Network Mode

For development and testing without host network access:

```bash
docker run --rm -it beatrice:latest
```

## Resource Management

### Memory and CPU Limits

```yaml
deploy:
  resources:
    limits:
      memory: 4G
      cpus: '4.0'
    reservations:
      memory: 2G
      cpus: '2.0'
```

### Huge Pages

For DPDK performance, configure huge pages on the host:

```bash
# Configure huge pages
echo 1024 > /proc/sys/vm/nr_hugepages

# Mount huge pages
mkdir -p /dev/hugepages
mount -t hugetlbfs nodev /dev/hugepages
```

## Security Considerations

### Privileged Mode

Beatrice requires privileged mode for:
- DPDK device access
- Network interface access
- eBPF program loading
- Huge page access

### Alternative: Capabilities

For production, consider using specific capabilities instead of privileged mode:

```bash
docker run --rm -it \
  --cap-add=NET_ADMIN \
  --cap-add=NET_RAW \
  --cap-add=SYS_ADMIN \
  --device=/dev/vfio/vfio \
  beatrice:latest
```

## Development Workflow

### 1. Local Development

```bash
# Start development container
docker-compose up -d beatrice-dev

# Enter container
docker-compose exec beatrice-dev bash

# Make changes to source code
# Changes are reflected immediately due to volume mounting

# Build and test
cd /opt/beatrice
mkdir -p build && cd build
cmake -DENABLE_DPDK=ON -DBUILD_EXAMPLES=ON ..
make -j$(nproc)
make test
```

### 2. Testing

```bash
# Run automated tests
docker-compose up beatrice-test

# View test results
docker-compose logs beatrice-test
```

### 3. Performance Testing

```bash
# Run benchmarks
docker-compose up beatrice-benchmark

# Analyze results
docker-compose logs beatrice-benchmark
```

### 4. Production Deployment

```bash
# Build production image
docker build -t beatrice:production .

# Deploy
docker-compose -f docker-compose.prod.yml up -d
```

## Troubleshooting

### Common Issues

#### 1. Permission Denied

```bash
# Ensure privileged mode
docker run --privileged beatrice:latest

# Or use specific capabilities
docker run --cap-add=NET_ADMIN --cap-add=NET_RAW beatrice:latest
```

#### 2. Network Interface Not Found

```bash
# Use host network mode
docker run --network host beatrice:latest

# Check available interfaces
docker run --rm beatrice:latest beatrice_cli info --interfaces
```

#### 3. DPDK Initialization Failed

```bash
# Check huge pages
cat /proc/meminfo | grep Huge

# Configure huge pages
echo 1024 > /proc/sys/vm/nr_hugepages
```

#### 4. Build Failures

```bash
# Clean build
docker-compose exec beatrice-dev bash
cd /opt/beatrice
rm -rf build
mkdir build && cd build
cmake -DENABLE_DPDK=ON -DBUILD_EXAMPLES=ON ..
make -j$(nproc)
```

### Debug Commands

```bash
# Check container status
docker-compose ps

# View logs
docker-compose logs -f [service-name]

# Enter container
docker-compose exec [service-name] bash

# Check network
docker-compose exec beatrice ip link show

# Test CLI
docker-compose exec beatrice beatrice_cli info --system
```

## Best Practices

### 1. Resource Management

- Set appropriate memory and CPU limits
- Monitor resource usage
- Use huge pages for DPDK performance

### 2. Security

- Use specific capabilities instead of privileged mode when possible
- Limit container access to necessary resources
- Regular security updates

### 3. Performance

- Use host network mode for production
- Configure huge pages
- Optimize DPDK EAL arguments

### 4. Monitoring

- Enable health checks
- Monitor logs
- Track performance metrics

## Integration

### CI/CD Pipeline

```yaml
# Example GitHub Actions workflow
- name: Build and Test
  run: |
    docker-compose up -d beatrice-test
    docker-compose logs beatrice-test
    docker-compose down
```

### Kubernetes

```yaml
# Example Kubernetes deployment
apiVersion: apps/v1
kind: Deployment
metadata:
  name: beatrice
spec:
  replicas: 1
  selector:
    matchLabels:
      app: beatrice
  template:
    metadata:
      labels:
        app: beatrice
    spec:
      containers:
      - name: beatrice
        image: beatrice:latest
        securityContext:
          privileged: true
        ports:
        - containerPort: 8080
```

## Support

For Docker-related issues:

1. Check the troubleshooting section
2. Review logs: `docker-compose logs [service-name]`
3. Verify configuration
4. Check host system requirements

## Contributing

To improve Docker support:

1. Test with different configurations
2. Add new services as needed
3. Optimize build process
4. Improve security configurations 