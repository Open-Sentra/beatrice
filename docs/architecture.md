# Architecture

## High-Level Overview

Beatrice SDK is designed for high-performance network packet capture and analysis in user-space on Linux.

### Core Components

- **ICaptureBackend**: Abstract interface for capture backends (e.g., AF_XDP, DPDK)
- **Packet**: Zero-copy data structure for packets
- **PluginManager**: Manages loading and unloading of plugins
- **IPacketPlugin**: Interface for packet processing plugins
- **BeatriceContext**: Main context that orchestrates backend and plugins

### Backends

- AF_XDPBackend: Uses XDP sockets for zero-copy capture
- DPDKBackend: Uses DPDK for high-performance capture (future)

### Plugins

Plugins are shared libraries (.so) that implement IPacketPlugin.

They can process packets for analysis, decoding, etc.

## Supported NICs

- Intel i40e, ixgbe, ice
- Mellanox ConnectX-5 and above
- Broadcom NetXtreme (future support)
