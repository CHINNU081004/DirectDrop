#include "Protocol.hpp"

#include <QtEndian>
#include <cstring>

namespace directdrop {

static constexpr int kHeaderSize = 16; // 4 + 2 + 2 + 2 + 2(reserved/flags_hi) -> let's use 4+2+2+2+2+4 = 16: magic(4) + maj(2) + min(2) + type(2) + flags(2) + len(4)

QByteArray encodeFrame(MessageType type, const QByteArray& payload, uint32_t flags) {
    QByteArray out;
    out.resize(kHeaderSize + payload.size());
    auto* ptr = reinterpret_cast<uchar*>(out.data());
    qToBigEndian<uint32_t>(kProtocolMagic, ptr);
    qToBigEndian<uint16_t>(kProtocolVersionMajor, ptr + 4);
    qToBigEndian<uint16_t>(kProtocolVersionMinor, ptr + 6);
    qToBigEndian<uint16_t>(static_cast<uint16_t>(type), ptr + 8);
    qToBigEndian<uint16_t>(static_cast<uint16_t>(flags & 0xFFFF), ptr + 10);
    qToBigEndian<uint32_t>(static_cast<uint32_t>(payload.size()), ptr + 12);
    if (!payload.isEmpty()) {
        std::memcpy(ptr + kHeaderSize, payload.constData(), static_cast<size_t>(payload.size()));
    }
    return out;
}

QByteArray encodeJsonFrame(MessageType type, const QJsonObject& obj, uint32_t flags) {
    return encodeFrame(type, QJsonDocument(obj).toJson(QJsonDocument::Compact), flags);
}

bool tryDecodeFrame(QByteArray& buffer, FrameHeader& outHeader, QByteArray& outPayload, bool& outFatalError, QString& outErrorMsg) {
    outFatalError = false;
    if (buffer.size() < kHeaderSize) {
        return false;
    }

    const auto* ptr = reinterpret_cast<const uchar*>(buffer.constData());
    uint32_t magic = qFromBigEndian<uint32_t>(ptr);
    if (magic != kProtocolMagic) {
        outFatalError = true;
        outErrorMsg = QStringLiteral("Invalid protocol magic: 0x%1").arg(magic, 8, 16, QLatin1Char('0'));
        return false;
    }

    uint16_t maj = qFromBigEndian<uint16_t>(ptr + 4);
    uint16_t min = qFromBigEndian<uint16_t>(ptr + 6);
    uint16_t typeRaw = qFromBigEndian<uint16_t>(ptr + 8);
    uint16_t flags = qFromBigEndian<uint16_t>(ptr + 10);
    uint32_t len = qFromBigEndian<uint32_t>(ptr + 12);

    if (len > kMaxFrameSize) {
        outFatalError = true;
        outErrorMsg = QStringLiteral("Frame payload too large: %1 bytes").arg(len);
        return false;
    }

    if (buffer.size() < kHeaderSize + static_cast<int>(len)) {
        return false; // Wait for more data
    }

    outHeader.magic = magic;
    outHeader.versionMajor = maj;
    outHeader.versionMinor = min;
    outHeader.type = static_cast<MessageType>(typeRaw);
    outHeader.flags = flags;
    outHeader.payloadLength = len;

    outPayload = buffer.mid(kHeaderSize, static_cast<int>(len));
    buffer.remove(0, kHeaderSize + static_cast<int>(len));
    return true;
}

QString transportTypeToString(TransportType t) {
    switch (t) {
        case TransportType::WifiDirect: return QStringLiteral("Wi-Fi Direct (Native P2P)");
        case TransportType::LocalWifiFallback: return QStringLiteral("Local Wi-Fi (Fallback)");
        case TransportType::HotspotFallback: return QStringLiteral("Phone Hotspot (Fallback)");
        case TransportType::Unknown: return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

QString zeroByteClassToString(ZeroByteClass c) {
    switch (c) {
        case ZeroByteClass::NotZeroByte: return QStringLiteral("Non-empty");
        case ZeroByteClass::LegitimateEmpty: return QStringLiteral("Legitimate empty file");
        case ZeroByteClass::UnexpectedZero: return QStringLiteral("Unexpected zero-byte read");
        case ZeroByteClass::PartialTransfer: return QStringLiteral("Partial transfer");
        case ZeroByteClass::SourceChanged: return QStringLiteral("Source changed during transfer");
        case ZeroByteClass::HashMismatch: return QStringLiteral("SHA-256 hash mismatch");
        case ZeroByteClass::FailedTransfer: return QStringLiteral("Failed transfer");
    }
    return QStringLiteral("Unknown");
}

} // namespace directdrop
