# DirectDrop

High-performance, verified, native **Wi-Fi Direct** file-transfer system between Android (Realme P3 Pro, Android 16) and Ubuntu Desktop (Acer Aspire E5-575, Ubuntu 26.04.1 LTS).

## Project Structure

- `protocol/` — Shared C++20/Qt 6 protocol framing (`DDP1`), path traversal validator, and atomic `SafeFileWriter` with SHA-256 verification and chunk-level resume.
- `desktop/` — Native Qt 6 Widgets receiver (`directdrop_desktop`) and Wi-Fi Direct D-Bus diagnostic tool (`directdrop_p2p_diag`).
- `android/` — Native Kotlin + Material 3 Android Studio/Gradle project using `WifiP2pManager`, Storage Access Framework (`OpenDocumentTree`), and TLS 1.3.
- `tests/` — Automated QtTest suite (`directdrop_tests`) covering protocol framing, path traversal, zero-byte classification, resume, and end-to-end TLS 1.3 transfer.
- `docs/` — Protocol and security specifications.

## Building & Running (Ubuntu Desktop)

```bash
cmake -S desktop -B desktop/build -G Ninja
cmake --build desktop/build
ctest --test-dir desktop/build --output-on-failure
```

Run the Wi-Fi Direct diagnostic scan:
```bash
./desktop/build/directdrop_p2p_diag 5
```

Launch the desktop receiver:
```bash
./desktop/build/directdrop_desktop
```

## Building (Android)

JDK and Android SDK are not pre-installed on this host. Open `DirectDrop/android/` in Android Studio (or install OpenJDK 17 + Android SDK Platform 36) and run:
```bash
./gradlew assembleDebug
```
