# DirectDrop v1.2.0

High-performance, verified, native **Wi-Fi Direct** file-transfer system between Android (API 29–36) and Linux Desktop (Ubuntu/Debian/Fedora/Arch, C++20 + Qt 6).

## Architecture & Liquid-Glass Motion System

- **Android** (`android/`): Native Kotlin + Views with damped spring physics (`Spring1D`), touch-tracking specular highlights, `LiquidBandSelectorView` (`Auto / 2.4 GHz / 5 GHz`), `LiquidDropProgressView`, `LiquidDeviceCardView` with custom touch-origin organic `LiquidRippleState`, `RadarScanView`, `TransferService` foreground service, and an In-App File Manager (`InAppFileManager`, `BrowserAdapter` with async thumbnails & Grid/List + Sort + Search).
- **Linux Desktop** (`desktop/`): Native C++20 / Qt 6 Widgets with `Fusion` style, `LiquidGlassPhysics` (`Spring1D`, `LiquidRipple`), `LiquidBandSelector`, `LiquidDropProgressBar`, `LiquidDeviceCard` (hover highlight + pointer-origin liquid ripple), `RadarScanWidget`, `WifiP2pBackend` (NetworkManager D-Bus), `TlsReceiverServer` (TLS 1.3 + 6-digit SAS pairing), and `TlsSenderClient`.
- **Shared Protocol** (`protocol/`): `DDP1` binary framing, path-traversal validation, `.ddpart` streaming, SHA-256 verification, atomic rename, and chunk-level resume.

## Build & Install

### Linux Desktop
```bash
cmake -S desktop -B desktop/build -G Ninja
cmake --build desktop/build
ctest --test-dir desktop/build --output-on-failure
sudo apt install "./releases/directdrop_1.2.0_amd64.deb"
```

### Android
```bash
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Release Artifacts (`releases/`)
- `directdrop_1.2.0_amd64.deb`
- `directdrop-1.2.0-debug.apk`
- `SHA256SUMS.txt`

## Known Limitations
- Optical refraction is simulated via layered radial gradients, specular rim lighting, and organic displaced wave paths rather than true background framebuffer refraction.
- Hardware Wi-Fi Direct group formation depends on driver/firmware channel concurrency (`ath10k_pci` verified for D-Bus discovery).
