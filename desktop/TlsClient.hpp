#pragma once

#include "Protocol.hpp"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QtEndian>

namespace directdrop {

class TlsSenderClient : public QObject {
    Q_OBJECT
public:
    explicit TlsSenderClient(QObject* parent = nullptr) : QObject(parent) {}

    bool sendFiles(const QString& host, quint16 port, const QStringList& localPaths, const QString& baseDir, QString* outError = nullptr) {
        QSslSocket sock;
        QSslConfiguration cfg = sock.sslConfiguration();
        cfg.setProtocol(QSsl::TlsV1_3OrLater);
        cfg.setPeerVerifyMode(QSslSocket::VerifyNone);
        sock.setSslConfiguration(cfg);

        sock.connectToHostEncrypted(host, port);
        if (!sock.waitForEncrypted(8000)) {
            if (outError) *outError = QStringLiteral("TLS connect failed: %1").arg(sock.errorString());
            return false;
        }

        sock.write(encodeJsonFrame(MessageType::Hello, {
            {QStringLiteral("deviceId"), QStringLiteral("ubuntu-desktop")},
            {QStringLiteral("deviceName"), QSysInfo::machineHostName()}
        }));
        sock.write(encodeJsonFrame(MessageType::PairRequest, {{QStringLiteral("nonce"), QStringLiteral("desktop-send")}}));

        QJsonArray manifestFiles;
        QList<QPair<QString, QString>> items; // localPath, relPath
        for (const QString& p : localPaths) {
            QFileInfo fi(p);
            QString rel = baseDir.isEmpty() ? fi.fileName() : QDir(baseDir).relativeFilePath(p);
            items.append({p, rel});
            manifestFiles.append(QJsonObject{{QStringLiteral("relativePath"), rel}, {QStringLiteral("size"), fi.size()}});
        }

        sock.write(encodeJsonFrame(MessageType::Manifest, {
            {QStringLiteral("jobId"), QStringLiteral("desktop-%1").arg(QDateTime::currentMSecsSinceEpoch())},
            {QStringLiteral("files"), manifestFiles}
        }));
        sock.flush();

        int idx = 0;
        for (const auto& pair : items) {
            QFile f(pair.first);
            if (!f.open(QIODevice::ReadOnly)) continue;
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(&f);
            QString sha = QString::fromLatin1(hash.result().toHex());
            f.seek(0);

            sock.write(encodeJsonFrame(MessageType::FileStart, {
                {QStringLiteral("relativePath"), pair.second},
                {QStringLiteral("size"), f.size()},
                {QStringLiteral("sha256"), sha}
            }));

            uint64_t offset = 0;
            while (!f.atEnd()) {
                QByteArray chunk = f.read(kDefaultChunkSize);
                QByteArray payload(8 + chunk.size(), '\0');
                qToBigEndian<uint64_t>(offset, reinterpret_cast<uchar*>(payload.data()));
                std::memcpy(payload.data() + 8, chunk.constData(), static_cast<size_t>(chunk.size()));
                sock.write(encodeFrame(MessageType::FileChunk, payload));
                offset += static_cast<uint64_t>(chunk.size());
                emit sendProgress(++idx, items.size(), pair.second);
            }
            sock.write(encodeJsonFrame(MessageType::FileEnd, {}));
            sock.flush();
        }

        sock.write(encodeJsonFrame(MessageType::JobComplete, {}));
        sock.flush();
        sock.waitForBytesWritten(5000);
        sock.disconnectFromHost();
        return true;
    }

signals:
    void sendProgress(int current, int total, const QString& fileName);
};

} // namespace directdrop
