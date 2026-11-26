# DirectDrop v1.2.2

High-performance, verified, native **Wi-Fi Direct** file-transfer system between Android (API 29–36) and Linux Desktop (Ubuntu/Debian/Fedora/Arch, C++20 + Qt 6).

## What's New in v1.2.2
- **Removed Moving Interior Hotspot**: Removed the pointer/touch-following `RadialGradient` / `QRadialGradient` hotspot from `TransparentGlassPainter` (`LiquidGlassEngine.kt`) and `drawTransparentGlassLens` (`LiquidGlassPhysics.hpp`). Replaced it with a stable, restrained top-edge inner sheen and directional rim highlight so the glass remains transparent and readable.
- **Fixed Linux Sidebar Hover Lens Lifecycle** (`desktop/LiquidSidebar.hpp`): Separated `m_activeIndex` from `m_hoveredIndex`, added child-button `Enter` / `Leave` / `MouseMove` event filtering, and ensured the hover lens snaps cleanly to hovered item bounds and fades out when leaving the sidebar.

## Build & Install

### Linux Desktop
```bash
cmake -S desktop -B desktop/build -G Ninja
cmake --build desktop/build --parallel
ctest --test-dir desktop/build --output-on-failure
sudo apt install "./releases/directdrop_1.2.2_amd64.deb"
```

### Android
```bash
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Release Artifacts (`releases/`)
- `directdrop_1.2.2_amd64.deb` (v1.2.0 & v1.2.1 preserved)
- `directdrop-1.2.2-debug.apk` (v1.2.0 & v1.2.1 preserved)
- `SHA256SUMS.txt`

## Known Limitations
- True optical background refraction is approximated via layered transparent neutral glass interiors, directional rim lighting, and organic displaced wave paths because Qt Widgets and Android Views do not expose a live behind-widget framebuffer sampler across arbitrary sibling layers.
- Hardware Wi-Fi Direct group formation depends on driver/firmware channel concurrency (`ath10k_pci` verified for D-Bus discovery).
