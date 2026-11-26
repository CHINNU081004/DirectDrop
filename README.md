# DirectDrop v1.4.1

High-performance, verified, native **Wi-Fi Direct** & **Through Wi-Fi Router (Secure LAN TLS 1.3)** file-transfer system between Android (API 29–36) and Linux Desktop (C++20 + Qt 6).

## What's New in v1.4.1
- **Three-Layer Capability Model** (`desktop/AdaptiveWifiPolicy.hpp`, `android/.../BandCapabilityManager.kt`):
  - Strictly separates **Wi-Fi Hardware/OS Support**, **Wi-Fi Direct Group-Band Support**, and **Router/LAN Transfer Capability** (`Supported`, `Unsupported`, `PermissionRequired`, `Unknown`). Missing `iw` or missing permissions report `Unknown` / `PermissionRequired` rather than false `Unsupported`.
- **Transport Selector & Two-Device Compatibility Comparison**:
  - Explicit **Direct (Wi-Fi Direct)** vs **Through Wi-Fi Router (LAN)** transport selector paired with live **You vs Peer** capability comparison, **Expected Performance** (link rate + conservative TCP range vs unknown), and real TCP reachability checks (`Verify LAN`).
  - Correctly handles dual/tri-band routers bridging endpoints on different bands (e.g. 6 GHz + 5 GHz) without falsely rejecting valid LAN paths.
- **Dynamic In-App "Learn More" Page** (`desktop/LearnMorePage.hpp`, `android/.../learnMorePage`):
  - Explains 2.4/5/6 GHz trade-offs, why 6 GHz Wi-Fi HW can work while Wi-Fi Direct does not, peer compatibility, router effects, and device-specific missing capabilities.

## What's New in v1.4.0
- **Four-Option Liquid-Glass Band Selector (`Auto | 2.4 GHz | 5 GHz | 6 GHz`)**:
  - Extended `LiquidBandSelector` (desktop) and `LiquidBandSelectorView` (Android) to all 4 options with persistent preference storage (`QSettings` / `SharedPreferences`).
- **Central Capability Detection & Honest Band Negotiation** (`desktop/AdaptiveWifiPolicy.hpp`, `android/.../BandCapabilityManager.kt`):
  - Probes real local capabilities (`iw list` + `iw dev <iface> link` on Linux; `WifiManager.is5GHzBandSupported` / `is6GHzBandSupported` / `WifiInfo.frequency` on Android API 30+).
  - Distinguishes adapter support, transport band-control exposure, requested preference, and verified active frequency. Never silently overwrites manual preferences or fakes unsupported bands.
- **Secure Local-Network Fallback** (`desktop/LanDiscovery.hpp`, `desktop/MainWindow.cpp`, `android/.../MainActivity.kt`):
  - Adds UDP local-network peer discovery (`47292`) and manual IP:Port TLS 1.3 + SAS pairing fallback when Wi-Fi Direct is unavailable or when transferring over an existing 5 GHz / 6 GHz router link.
- **GitHub Actions CI**: Added `.github/workflows/ci.yml` for Linux Desktop (CMake + CTest) and Android APK builds.

## Compatibility Matrix

| Platform | Architecture | Transports | Band Detection | Manual Band Control | Build / Test Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Linux Desktop (Ubuntu/Debian/Fedora/Arch) | x86_64 (ARM64 source-ready) | Wi-Fi Direct (NM D-Bus), Secure LAN (TLS 1.3) | `iw list` (2.4/5/6 GHz) & `iw dev link` | Auto + Secure LAN telemetry (NM P2P delegates channel to `wpa_supplicant`) | Built & CTest verified on x86_64 |
| Android (API 29–36) | ARM64 / ARMv7 | Wi-Fi Direct (`WifiP2pManager`), Secure LAN (TLS 1.3) | `is5GHzBandSupported`, `is6GHzBandSupported` (API 30+), `WifiInfo.frequency` | `Auto`, `2.4 GHz`, `5 GHz` via `WifiP2pConfig`; `6 GHz` via Secure LAN over 6 GHz AP | Built & verified on Realme P3 Pro (`RMX5032`, Android 16) |

## What's New in v1.3.1
- **Desktop Button Sizing & Responsive Wrapping** (`desktop/FlowLayout.hpp`, `desktop/FileManagerWidget.cpp`, `desktop/MainWindow.cpp`):
  - Replaced rigid single-row toolbars with `FlowLayout` and `QSizePolicy::Minimum` so every button label (`Select All`, `Clear`, `Preview`, `New Folder`, `Rename`, `Delete`, `Add to Queue`, `Send Queued →`, `Connect Device →`, `Connect Selected`) fits cleanly at `1366×768` and narrower (`780×560`) widths without clipping.
- **Movable Liquid-Glass Category Selector on Android** (`android/.../LiquidSegmentedSelectorView.kt`, `MainActivity.kt`, `activity_main.xml`):
  - Replaced emoji-only category tabs with a spring-animated translucent liquid-glass segmented selector (`Files`, `Images`, `Videos`, `Audio`, `Apps`) featuring horizontal scroll support, velocity stretch, reduced-motion support, per-category scroll preservation, and empty-state feedback.
- **Universal Liquid Glass Design Tokens**:
  - Unified `LiquidSegmentedSelector` (`desktop/LiquidSegmentedSelector.hpp`) across File Manager tabs and Adaptive Wi-Fi band selection on desktop, and `LiquidSegmentedSelectorView` + `LiquidGlassTokens` across Home tabs, File Selection categories, and neutral glass buttons on Android.

## What's New in v1.3.0
- **Native Desktop File Manager** (`desktop/FileManagerWidget.hpp`, `desktop/FileManagerWidget.cpp`):
  - **Images**, **Files**, and **Folders** segmented tabs inside the Files workspace.
  - Background directory enumeration (`QtConcurrent`), lazy thumbnail loading with a 250-item `QCache`, Grid/List toggle, hidden-files toggle, Search, Sort (Name/Size/Type/Date), Filter (All/Images/Videos/Documents/Archives), Volume switcher (Home/Downloads/Pictures/Mounted drives), and in-app Preview.
  - Safe local operations (**New Folder**, **Rename**, **Delete** with explicit confirmation and symlink guard) kept strictly separate from **copy-only** sending via the DirectDrop Send Queue.
- **Adaptive Wi-Fi Band & Capability Policy** (`desktop/AdaptiveWifiPolicy.hpp`, `android/.../WifiP2pController.kt`):
  - Probes real local adapter capabilities (`iw list` on Linux, `WifiManager.is5GHzBandSupported` on Android).
  - **Auto (Recommended)** prefers 5 GHz when supported and falls back to 2.4 GHz on unsupported/failed links; **2.4 GHz (Range)** and **5 GHz (Speed)** remain user-selectable.
- **Complete Liquid-Glass Consistency**: Transparent neutral glass lenses, subtle depth shadows, fixed soft top-edge sheen, and restrained rim highlights across all desktop and mobile screens (no moving interior hotspot).

## Build & Install

### Linux Desktop
```bash
cmake -S desktop -B desktop/build -G Ninja
cmake --build desktop/build --parallel
ctest --test-dir desktop/build --output-on-failure
sudo apt install "./releases/directdrop_1.4.1_amd64.deb"
```

### Android
```bash
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Release Artifacts (`releases/`)
- `directdrop_1.4.1_amd64.deb` (earlier versions preserved)
- `directdrop-1.4.1-debug.apk` (earlier versions preserved)
- `SHA256SUMS.txt`

## Known Limitations
- Wi-Fi Direct operates on a negotiated single group channel; simultaneous dual-band aggregation (2.4 GHz + 5 GHz combined) is not implemented.
- On Linux, NetworkManager's `wifi-p2p` D-Bus profile delegates channel negotiation to `wpa_supplicant` rather than forcing a fixed channel from client D-Bus properties.
- Optical refraction remains a layered transparent-glass approximation.
