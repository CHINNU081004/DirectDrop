#include "SafeFileWriter.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

namespace directdrop {

SafeFileWriter::~SafeFileWriter() {
    if (m_file.isOpen()) {
        m_file.close();
    }
}

bool SafeFileWriter::begin(const QString& finalPath, uint64_t expectedSize, const QString& expectedSha256Hex, bool allowResume, QString* outError) {
    m_finalPath = finalPath;
    m_tempPath = finalPath + QStringLiteral(".ddpart");
    m_expectedSize = expectedSize;
    m_expectedSha256Hex = expectedSha256Hex.toLower();
    m_bytesWritten = 0;
    m_hash.reset();

    QFileInfo finalInfo(finalPath);
    QDir parentDir = finalInfo.dir();
    if (!parentDir.exists() && !parentDir.mkpath(QStringLiteral("."))) {
        if (outError) *outError = QStringLiteral("Failed to create parent directory: %1").arg(parentDir.absolutePath());
        return false;
    }

    QStorageInfo storage(parentDir.absolutePath());
    if (storage.isValid() && storage.bytesAvailable() >= 0) {
        if (static_cast<uint64_t>(storage.bytesAvailable()) < expectedSize + 1024 * 1024) {
            if (outError) *outError = QStringLiteral("Insufficient disk space on %1").arg(storage.rootPath());
            return false;
        }
    }

    if (allowResume && QFileInfo::exists(m_tempPath)) {
        m_file.setFileName(m_tempPath);
        if (m_file.open(QIODevice::ReadWrite)) {
            qint64 existingSize = m_file.size();
            if (existingSize > 0 && static_cast<uint64_t>(existingSize) <= expectedSize) {
                // Re-hash existing bytes so we never trust unverified partial data blindly
                m_file.seek(0);
                QByteArray buf;
                while (!m_file.atEnd()) {
                    buf = m_file.read(256 * 1024);
                    m_hash.addData(buf);
                }
                m_bytesWritten = static_cast<uint64_t>(existingSize);
                m_active = true;
                return true;
            }
            m_file.close();
        }
    }

    m_file.setFileName(m_tempPath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (outError) *outError = QStringLiteral("Cannot open temp file %1: %2").arg(m_tempPath, m_file.errorString());
        return false;
    }
    m_active = true;
    return true;
}

bool SafeFileWriter::writeChunk(uint64_t offset, const QByteArray& data, QString* outError) {
    if (!m_active || !m_file.isOpen()) {
        if (outError) *outError = QStringLiteral("Writer not active");
        return false;
    }
    if (offset != m_bytesWritten) {
        if (outError) *outError = QStringLiteral("Invalid chunk offset %1 (expected %2)").arg(offset).arg(m_bytesWritten);
        return false;
    }
    if (m_bytesWritten + static_cast<uint64_t>(data.size()) > m_expectedSize) {
        if (outError) *outError = QStringLiteral("Chunk exceeds expected file size");
        return false;
    }
    qint64 written = m_file.write(data);
    if (written != data.size()) {
        if (outError) *outError = QStringLiteral("Short write: %1").arg(m_file.errorString());
        return false;
    }
    m_hash.addData(data);
    m_bytesWritten += static_cast<uint64_t>(written);
    return true;
}

bool SafeFileWriter::finalize(ZeroByteClass& outZeroClass, QString* outError) {
    if (!m_active || !m_file.isOpen()) {
        outZeroClass = ZeroByteClass::FailedTransfer;
        if (outError) *outError = QStringLiteral("Writer not active");
        return false;
    }

    m_file.flush();
    m_file.close();
    m_active = false;

    if (m_bytesWritten != m_expectedSize) {
        outZeroClass = (m_bytesWritten == 0 && m_expectedSize > 0)
            ? ZeroByteClass::UnexpectedZero
            : ZeroByteClass::PartialTransfer;
        if (outError) *outError = QStringLiteral("Size mismatch: got %1, expected %2").arg(m_bytesWritten).arg(m_expectedSize);
        return false;
    }

    QString actualHash = QString::fromLatin1(m_hash.result().toHex()).toLower();
    if (!m_expectedSha256Hex.isEmpty() && actualHash != m_expectedSha256Hex) {
        outZeroClass = ZeroByteClass::HashMismatch;
        if (outError) *outError = QStringLiteral("SHA-256 mismatch: got %1, expected %2").arg(actualHash, m_expectedSha256Hex);
        return false;
    }

    if (QFile::exists(m_finalPath)) {
        if (!QFile::remove(m_finalPath)) {
            outZeroClass = ZeroByteClass::FailedTransfer;
            if (outError) *outError = QStringLiteral("Cannot replace existing file %1").arg(m_finalPath);
            return false;
        }
    }

    if (!QFile::rename(m_tempPath, m_finalPath)) {
        outZeroClass = ZeroByteClass::FailedTransfer;
        if (outError) *outError = QStringLiteral("Atomic rename failed: %1 -> %2").arg(m_tempPath, m_finalPath);
        return false;
    }

    outZeroClass = (m_expectedSize == 0) ? ZeroByteClass::LegitimateEmpty : ZeroByteClass::NotZeroByte;
    return true;
}

void SafeFileWriter::abortAndCleanup(bool keepPartialForResume) {
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
    m_active = false;
    if (!keepPartialForResume && !m_tempPath.isEmpty()) {
        QFile::remove(m_tempPath);
    }
}

} // namespace directdrop
