#pragma once

#include "Protocol.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QString>
#include <cstdint>

namespace directdrop {

class SafeFileWriter {
public:
    SafeFileWriter() = default;
    ~SafeFileWriter();

    bool begin(const QString& finalPath, uint64_t expectedSize, const QString& expectedSha256Hex, bool allowResume, QString* outError = nullptr);
    bool writeChunk(uint64_t offset, const QByteArray& data, QString* outError = nullptr);
    bool finalize(ZeroByteClass& outZeroClass, QString* outError = nullptr);
    void abortAndCleanup(bool keepPartialForResume = true);

    uint64_t bytesWritten() const { return m_bytesWritten; }
    QString tempPath() const { return m_tempPath; }
    QString finalPath() const { return m_finalPath; }

private:
    QString m_finalPath;
    QString m_tempPath;
    uint64_t m_expectedSize = 0;
    QString m_expectedSha256Hex;
    uint64_t m_bytesWritten = 0;
    QFile m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    bool m_active = false;
};

} // namespace directdrop
