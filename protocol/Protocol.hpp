#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <cstdint>

namespace directdrop {

inline constexpr uint32_t kProtocolMagic = 0x44445031; // "DDP1"
inline constexpr uint16_t kProtocolVersionMajor = 1;
inline constexpr uint16_t kProtocolVersionMinor = 0;
inline constexpr uint32_t kMaxFrameSize = 16 * 1024 * 1024; // 16 MiB max frame
inline constexpr uint32_t kDefaultChunkSize = 256 * 1024;   // 256 KiB
inline constexpr int kMaxManifestEntries = 200000;

enum class MessageType : uint16_t {
    Hello = 0x0001,
    HelloAck = 0x0002,
    PairRequest = 0x0010,
    PairChallenge = 0x0011,
    PairConfirm = 0x0012,
    PairReject = 0x0013,
    Manifest = 0x0020,
    ManifestAck = 0x0021,
    FileStart = 0x0030,
    FileChunk = 0x0031,
    FileEnd = 0x0032,
    FileAck = 0x0033,
    ResumeQuery = 0x0040,
    ResumeAck = 0x0041,
    JobComplete = 0x0050,
    Ping = 0x0060,
    Pong = 0x0061,
    Error = 0x00FF
};

enum class ZeroByteClass {
    NotZeroByte,
    LegitimateEmpty,
    UnexpectedZero,
    PartialTransfer,
    SourceChanged,
    HashMismatch,
    FailedTransfer
};

enum class ConflictPolicy {
    KeepBothRename,
    SkipExisting,
    ReplaceWithConfirm
};

enum class TransportType {
    WifiDirect,
    LocalWifiFallback,
    HotspotFallback,
    Unknown
};

struct FrameHeader {
    uint32_t magic = kProtocolMagic;
    uint16_t versionMajor = kProtocolVersionMajor;
    uint16_t versionMinor = kProtocolVersionMinor;
    MessageType type = MessageType::Error;
    uint32_t payloadLength = 0;
    uint32_t flags = 0;
};

QByteArray encodeFrame(MessageType type, const QByteArray& payload, uint32_t flags = 0);
QByteArray encodeJsonFrame(MessageType type, const QJsonObject& obj, uint32_t flags = 0);
bool tryDecodeFrame(QByteArray& buffer, FrameHeader& outHeader, QByteArray& outPayload, bool& outFatalError, QString& outErrorMsg);

QString transportTypeToString(TransportType t);
QString zeroByteClassToString(ZeroByteClass c);

} // namespace directdrop
