# DirectDrop v1.3.0

High-performance, verified, native **Wi-Fi Direct** file-transfer system between Android (API 29–36) and Linux Desktop (Ubuntu/Debian/Fedora/Arch, C++20 + Qt 6).

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
sudo apt install "./releases/directdrop_1.3.0_amd64.deb"
```

### Android
```bash
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Release Artifacts (`releases/`)
- `directdrop_1.3.0_amd64.deb` (earlier versions preserved)
- `directdrop-1.3.0-debug.apk` (earlier versions preserved)
- `SHA256SUMS.txt`

## Known Limitations
- Wi-Fi Direct operates on a negotiated single group channel; simultaneous dual-band aggregation (2.4 GHz + 5 GHz combined) is not implemented.
- On Linux, NetworkManager's `wifi-p2p` D-Bus profile delegates channel negotiation to `wpa_supplicant` rather than forcing a fixed channel from client D-Bus properties.
- Optical refraction remains a layered transparent-glass approximation.
