# DirectDrop v1.2.1

High-performance, verified, native **Wi-Fi Direct** file-transfer system between Android (API 29–36) and Linux Desktop (Ubuntu/Debian/Fedora/Arch, C++20 + Qt 6).

## What's New in v1.2.1
- **Transparent Liquid-Glass Rendering**: Replaced colored cyan/blue fills with predominantly transparent, colorless dimensional glass lenses (`TransparentGlassPainter` / `drawTransparentGlassLens`), subtle depth shadows, directional luminous rims, and moving specular glints across `LiquidBandSelector`, `LiquidDropProgressBar`, `LiquidDeviceCard`, and `LiquidRipple`.
- **Linux Left Sidebar + Central Workspace** (`desktop/LiquidSidebar.hpp`, `desktop/MainWindow.cpp`): Persistent left navigation sidebar (**Overview**, **Files**, **Connect Device**, **Transfers**, **Storage**) with an input-transparent pointer-following liquid lens (`LiquidSidebarLens`) and responsive central workspace.

## Build & Install

### Linux Desktop
```bash
cmake -S desktop -B desktop/build -G Ninja
cmake --build desktop/build
ctest --test-dir desktop/build --output-on-failure
sudo apt install "./releases/directdrop_1.2.1_amd64.deb"
```

### Android
```bash
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Release Artifacts (`releases/`)
- `directdrop_1.2.1_amd64.deb` (and `directdrop_1.2.0_amd64.deb` preserved)
- `directdrop-1.2.1-debug.apk` (and `directdrop-1.2.0-debug.apk` preserved)
- `SHA256SUMS.txt`

## Known Limitations
- True optical background refraction is approximated via layered transparent neutral glass interiors, directional rim lighting, and organic displaced wave paths because Qt Widgets and Android View hierarchies do not expose a live behind-widget framebuffer sampler across arbitrary sibling layers without a full compositor/SceneGraph rewrite.
- Hardware Wi-Fi Direct group formation depends on driver/firmware channel concurrency (`ath10k_pci` verified for D-Bus discovery).
