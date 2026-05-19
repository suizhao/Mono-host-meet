# Mono-host Meet

A cross-platform desktop real-time video/audio conferencing client built on the [LiveKit](https://livekit.io/) WebRTC stack.

**C++17 · Qt 6.10 / QML · LiveKit C++ SDK · DXGI/D3D11 · Node.js · Docker**

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                 Presentation Layer                   │
│         QML UI  ←→  Controllers (Qt signals)        │
└──────────────────────┬──────────────────────────────┘
                       │ depends on
┌──────────────────────▼──────────────────────────────┐
│                   Domain Layer                       │
│   IBackendService  │  IRoomService  │  IDeviceService │
│   (abstract interfaces, no SDK deps)                 │
└──────────────────────┬──────────────────────────────┘
                       │ implemented by
┌──────────────────────▼──────────────────────────────┐
│               Infrastructure Layer                   │
│  BackendServiceQt  │  LiveKitRoomService             │
│  (HTTP/Qt Network) │  (LiveKit C++ SDK + capture)   │
│  DeviceServiceQt   │                                 │
│  (subprocess probe + JSON IPC)                      │
└─────────────────────────────────────────────────────┘
```

Dependency direction: `Presentation → Domain interfaces ← Infrastructure implementations`.

`AppContext` is the single composition root — QML never touches the SDK directly.

---

## Features

### Real-time Communication
- **Rooms**: JWT-authenticated connect/disconnect via LiveKit SFU
- **Microphone**: PCM capture at 48000 Hz / Int16, 20 ms frames, mute/unmute
- **Camera**: video track publish with RGBA test-pattern frames at 10 fps
- **Speaker**: remote audio mix playback via `QAudioSink` pipeline
- **Data channel**: host commands broadcast (disband room, mute-all)

### Screen Sharing (Windows)
- **Primary path**: DXGI Desktop Duplication (D3D11) — zero-copy texture capture
- **Fallback path**: GDI `BitBlt` with DIB Section for compatibility
- BGRA → RGBA per-pixel conversion, 30 fps encoding
- Frame downscale to 1280px for local preview when source exceeds it

### Multi-tile Video Grid
- Per-participant/per-track tiles (camera + screen share rendered independently)
- **Picture-in-Picture**: camera tile overlays screen-share tile bottom-right
- Placeholder tiles for participants without active video tracks
- 80 ms frame-rate throttle, 3-second staleness detection with auto-cleanup
- Tile priority: remote first → screen share before camera → lexicographic

### Rendering Pipeline
- Custom `QQuickImageProvider` (`image://multilive/<tileId>?v=<version>`)
- `QAbstractListModel` with incremental sync — only `dataChanged` on same tile IDs
- Replaces heavyweight base64 data URLs to reduce memory churn and flicker

### Device Enumeration (Subprocess Isolation)
- Qt Multimedia platform plugins run in a **separate binary** (`multilive_device_probe`)
- Main process communicates via `QProcess` + JSON stdout
- Crash containment: a multimedia plugin crash kills only the probe, never the UI
- Graceful fallback to placeholder devices on probe failure

### Session State Machine
- 5 implicit states: idle / joining / in-room / recording-in-flight / error
- Concurrent operation guards (`joinInProgress`, `recordingRequestInFlight`)
- Full UI session reset on leave (tiles, participants, errors, track state)
- Recording auto-stop on room leave (host only)

### Recording
- LiveKit Egress composite recording (mp4, `speaker` layout)
- Host-only start/stop with duplicate prevention

---

## Project Structure

```
MultiLive/
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp                          # Qt entry point, DI wiring
    ├── tools/
    │   └── device_probe_main.cpp          # Standalone device probe binary
    ├── app/
    │   ├── app_context.h/.cpp             # Dependency injection container
    │   └── env_config.h/.cpp              # Environment config (env vars)
    ├── domain/services/
    │   ├── backend_service.h              # IBackendService interface
    │   ├── room_service.h                 # IRoomService interface
    │   └── device_service.h               # IDeviceService interface
    ├── infra/
    │   ├── http/backend_service_qt.h/.cpp # REST API client (Qt Network)
    │   ├── rtc/livekit_room_service.h/.cpp # LiveKit C++ SDK adapter
    │   └── device/device_service_qt.h/.cpp # Device enumeration adapter
    └── presentation/
        ├── controllers/
        │   ├── home_controller.h/.cpp      # Room join/leave, recording, error
        │   ├── prejoin_controller.h/.cpp   # Custom join form state
        │   ├── room_controller.h/.cpp      # Media controls, device selection
        │   └── video_tile_model.h/.cpp     # QAbstractListModel for video grid
        └── qml/
            ├── App.qml                     # Root window + StackView
            ├── frame_image_provider.h/.cpp # QQuickImageProvider for tiles
            └── pages/HomePage.qml          # Main UI (controls + grid + settings)
```

---

## Prerequisites

| Dependency | Version | Notes |
|---|---|---|
| CMake | ≥ 3.16 | |
| Qt 6 | ≥ 6.10 | Modules: Quick, Qml, Network, Multimedia, Gui |
| LiveKit C++ SDK | 0.3.4 | Prebuilt, fetched via CMake module |
| MSVC / Clang | C++17 | **Release** build required for RTC (Debug CRT ABI conflict) |
| GITHUB_TOKEN | env var | For LiveKit SDK download from GitHub Packages |

### Windows-only dependencies
- DirectX 11 SDK (shipped with Windows SDK)

---

## Build

```bash
# Set LiveKit SDK access token
export GITHUB_TOKEN=<your-github-token>

# Configure (Release required for room connectivity)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Two binaries are produced:
- `appMultiLive` — the main desktop client
- `multilive_device_probe` — headless device enumeration helper

---

## Run

### 1. Start the backend stack

```bash
cd multilive-docker
docker compose up -d
```

This launches:

| Service | Port | Purpose |
|---|---|---|
| livekit-server | 7880–7881, 50001–50200/UDP | WebRTC SFU |
| token-service (Node.js) | 3000 | JWT issuance, room management, recording |
| redis | (internal) | LiveKit state store |
| egress | 8085 | Composite recording |

### 2. Launch the client

```bash
# Default endpoints point to local Docker services
./build/appMultiLive
```

### 3. Custom endpoints

```bash
BACKEND_BASE_URL=http://192.168.1.100:3000 \
LIVEKIT_URL=wss://192.168.1.100:7880 \
./build/appMultiLive
```

---

## Backend API (token-service)

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| GET | `/api/connection-details` | None | Get JWT token + server URL |
| POST | `/api/room/disband` | Host only | Delete room via LiveKit API |
| POST | `/api/room/mute-all` | Host only | Mute all participant mics |
| POST | `/api/recording/start` | Host only | Start composite Egress recording |
| POST | `/api/recording/stop` | Host only | Stop active recording |
| GET | `/api/room/debug` | None | List participants (debug) |

---

## Key Design Decisions

- **Clean Architecture**: domain interfaces are pure virtual; infra can be swapped for mock/testing
- **Debug/Release split**: Debug builds block RTC connectivity to avoid MSVC Debug CRT vs. prebuilt SDK ABI mismatch
- **Polling + events**: SDK callbacks drive immediate refresh via `QueuedConnection`; a background 80 ms `QTimer` polls as fallback
- **Thread safety**: remote frame callbacks use `std::weak_ptr<RemoteState>` (no raw `this`), `QMutex` guards shared tile state, frame-rate throttle prevents callback storms
- **Subprocess device probe**: Qt Multimedia lives in its own binary to contain platform plugin crashes
- **ImageProvider over data URLs**: video frames go to `QQuickImageProvider` rather than base64-encoded QML properties, reducing memory pressure and UI flicker

---

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE)
