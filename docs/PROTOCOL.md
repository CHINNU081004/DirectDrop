# DirectDrop Protocol Specification (DDP1)

## Framing
Every message is a 16-byte big-endian header followed by `payloadLength` bytes:
- `magic` (`uint32`): `0x44445031` (`"DDP1"`)
- `versionMajor` (`uint16`): `1`
- `versionMinor` (`uint16`): `0`
- `type` (`uint16`): Message type (`Hello`, `PairRequest`, `Manifest`, `FileStart`, `FileChunk`, `FileEnd`, `FileAck`, `ResumeAck`, `JobComplete`, `Ping`, `Pong`, `Error`)
- `flags` (`uint16`): Reserved (`0`)
- `payloadLength` (`uint32`): Max 16 MiB

## Security
- Transport: **TLS 1.3** (`QSsl::TlsV1_3OrLater` on desktop, `TLSv1.3` on Android).
- Pairing: 6-digit Short Authentication String (SAS) derived from `SHA-256(serverCertDigest || clientNonce || deviceId)` displayed on both devices for explicit user confirmation, preventing MITM attacks.
- Path Safety: All relative paths are validated to reject `..`, leading `/`, `\`, drive prefixes, and control characters.
- Integrity: Files are streamed to `<dest>.ddpart`, verified against expected byte size and SHA-256 digest, flushed, and atomically renamed.
