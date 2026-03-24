# S-Mouse

Cross-platform mouse and keyboard sharing over LAN. Control multiple computers (macOS & Windows) with a single mouse and keyboard — move your cursor to the screen edge and it seamlessly jumps to the next machine.

Similar to Synergy/Barrier, but built from scratch in modern C++20 with BLE proximity detection.

## Features

- **Mouse & Keyboard Sharing** — Move your cursor across machines as if they were one desktop
- **Clipboard Sync** — Copy on one machine, paste on the other (text, HTML, images)
- **Screen Edge Detection** — Automatic switching when your cursor hits the screen edge
- **BLE Proximity** — Auto-disconnect when you walk away, auto-reconnect when you return
- **Auto-Discovery** — Finds servers on your LAN automatically via UDP broadcast
- **Encrypted** — TLS 1.3 mandatory, PIN-based pairing, no plaintext option
- **Qt6 GUI** — Visual screen layout editor, system tray icon, settings dialog
- **Low Latency** — UDP for mouse movement (~137µs on localhost), TCP for keystrokes

## Supported Platforms

| Combination | Status |
|-------------|--------|
| Mac → Mac   | Implemented |
| Mac → Win   | Implemented |
| Win → Mac   | Implemented |
| Win → Win   | Implemented |

## Architecture

```
┌─────────────┐          LAN           ┌─────────────┐
│   Server    │◄──── TCP + UDP ────►│   Client    │
│ (your KB/M) │                        │ (other PC)  │
│             │  TCP: keys, clipboard  │             │
│  Captures   │  UDP: mouse movement   │  Injects    │
│  input      │  BLE: proximity detect │  input      │
└─────────────┘                        └─────────────┘
```

**Server** = the machine with your physical keyboard and mouse.
**Client** = machines that receive input from the server.

### Protocol

| Data | Transport | Why |
|------|-----------|-----|
| Mouse movement | UDP | Lowest latency, tolerates packet loss |
| Keystrokes | TCP | Must arrive in order |
| Clipboard | TCP | Reliable, can be large |
| Screen transitions | TCP | Must be reliable |

Custom binary protocol with 8-byte headers. No protobuf overhead.

### BLE Proximity (Auto-Disconnect/Reconnect)

| RSSI | State | Action |
|------|-------|--------|
| > -60 dBm | NEAR (~1-2m) | Allow connection |
| -60 to -80 | MEDIUM | Keep connection |
| < -80 dBm | FAR (~5m+) | Auto-disconnect |
| No signal | LOST | Auto-disconnect |

Falls back to TCP keepalive (3s timeout) on devices without Bluetooth.

## Build

### Requirements

- C++20 compiler (Clang 14+, GCC 12+, MSVC 2022+)
- CMake 3.21+
- OpenSSL 3.x
- Qt6 (optional, for GUI)
- macOS: Xcode Command Line Tools
- Windows: Visual Studio 2022

### macOS

```bash
# Install dependencies
brew install cmake openssl googletest qt@6

# Build (daemon only)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)

# Build with GUI
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DSMOUSE_BUILD_GUI=ON \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt@6
cmake --build build -j$(sysctl -n hw.ncpu)

# Run tests
ctest --test-dir build --output-on-failure
```

### Windows

```bash
# Install dependencies (vcpkg or manually)
vcpkg install openssl gtest qt6

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Usage

### GUI Mode

```bash
# macOS
open build/src/gui/smouse-gui.app

# Windows
build\src\gui\Release\smouse-gui.exe
```

1. Choose **Server** or **Client** mode
2. Server: Click **Start Server**
3. Client: Enter the server's IP address, click **Connect**
4. Drag screens in the layout editor to arrange them
5. Move your mouse to the edge — it jumps to the other machine

### CLI Mode

```bash
# Start server (machine with keyboard/mouse)
./build/src/daemon/smouse-daemon server

# Connect client (other machine)
./build/src/daemon/smouse-daemon client 192.168.1.100

# Options
./build/src/daemon/smouse-daemon server 24800          # custom port
./build/src/daemon/smouse-daemon client 192.168.1.100 --name "MacBook"
```

### macOS Permissions

S-Mouse requires **Accessibility** permission to capture and inject input events:

**System Settings → Privacy & Security → Accessibility** → Enable for `smouse-daemon` or `S-Mouse.app`

## Project Structure

```
src/
├── core/           # Platform-independent logic
│   ├── protocol    # Binary wire protocol (serialize/deserialize)
│   ├── server      # Server state machine (edge detect, routing)
│   ├── client      # Client state machine (inject, reconnect)
│   ├── keymap      # USB HID ↔ platform scancode translation
│   ├── screen_layout  # Screen arrangement & edge detection
│   └── clipboard   # Clipboard abstraction
├── net/            # Networking
│   ├── tcp_connection  # TCP with TCP_NODELAY
│   ├── udp_channel     # UDP for mouse events
│   ├── tls_context     # TLS 1.3 (OpenSSL)
│   ├── ble_proximity   # BLE RSSI proximity detection
│   └── discovery       # LAN auto-discovery (UDP broadcast)
├── platform/       # OS abstraction layer
│   ├── macos/      # CGEventTap, CGEventPost, CoreBluetooth, NSPasteboard
│   └── windows/    # Low-level hooks, SendInput, WinRT BLE, Clipboard API
├── gui/            # Qt6 GUI
│   ├── mainwindow       # Server/client connection UI
│   ├── screen_editor    # Drag-to-arrange screen layout widget
│   ├── settings_dialog  # Port, dead zone, auto-reconnect config
│   └── tray_icon        # System tray (green/yellow/red status)
└── daemon/         # Headless daemon (CLI)
```

## Tests

```bash
ctest --test-dir build --output-on-failure
```

| Suite | Tests | What |
|-------|-------|------|
| `test_protocol` | 20 | Serialization round-trips, edge cases |
| `test_screen_layout` | 10 | Edge detection, dead zones, multi-client |
| `test_net` | 7 | TCP/UDP send/receive, disconnect detection, latency benchmark |

## Security

- **TLS 1.3** on all TCP connections (OpenSSL)
- **ChaCha20-Poly1305** for UDP encryption (planned)
- **PIN-based pairing** — 6-digit PIN displayed on server, entered on client
- **Certificate fingerprint storage** — no re-pairing needed after first connection
- **No plaintext mode** — encryption is always on

## License

MIT
