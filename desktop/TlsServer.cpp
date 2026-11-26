#include "TlsServer.hpp"
#include "PathValidator.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSslConfiguration>
#include <QStandardPaths>
#include <QUuid>

namespace directdrop {

TlsReceiverServer::TlsReceiverServer(QObject* parent) : QTcpServer(parent) {
    m_destRoot = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + QStringLiteral("/DirectDrop");
}

TlsReceiverServer::~TlsReceiverServer() {
    stopListening();
}

bool TlsReceiverServer::ensureServerIdentity() {
    if (!m_cert.isNull() && !m_key.isNull()) return true;

    QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(cfgDir);
    QString certPath = cfgDir + QStringLiteral("/server_cert.pem");
    QString keyPath = cfgDir + QStringLiteral("/server_key.pem");

    if (!QFileInfo::exists(certPath) || !QFileInfo::exists(keyPath)) {
        QProcess proc;
        proc.start(QStringLiteral("openssl"), {
            QStringLiteral("req"), QStringLiteral("-x509"), QStringLiteral("-newkey"), QStringLiteral("ec"),
            QStringLiteral("-pkeyopt"), QStringLiteral("ec_paramgen_curve:prime256v1"),
            QStringLiteral("-keyout"), keyPath, QStringLiteral("-out"), certPath,
            QStringLiteral("-days"), QStringLiteral("365"), QStringLiteral("-nodes"),
            QStringLiteral("-subj"), QStringLiteral("/CN=DirectDrop-Desktop")
        });
        if (!proc.waitForFinished(10000) || proc.exitCode() != 0) {
            emit logMessage(QStringLiteral("OpenSSL cert generation failed: %1").arg(QString::fromUtf8(proc.readAllStandardError())));
            return false;
        }
    }

    QFile cf(certPath), kf(keyPath);
    if (!cf.open(QIODevice::ReadOnly) || !kf.open(QIODevice::ReadOnly)) return false;
    m_cert = QSslCertificate(&cf, QSsl::Pem);
    m_key = QSslKey(&kf, QSsl::Ec, QSsl::Pem);
    return !m_cert.isNull() && !m_key.isNull();
}

bool TlsReceiverServer::startListening(const QHostAddress& bindAddr, quint16 port, QString* outError) {
    if (!ensureServerIdentity()) {
        if (outError) *outError = QStringLiteral("Failed to initialize TLS 1.3 identity");
        return false;
    }
    if (!listen(bindAddr, port)) {
        if (outError) *outError = errorString();
        return false;
    }
    emit logMessage(QStringLiteral("TLS 1.3 receiver listening on %1:%2").arg(bindAddr.toString()).arg(port));
    return true;
}

void TlsReceiverServer::stopListening() {
    for (auto& sess : m_sessions) {
        if (sess->writer) sess->writer->abortAndCleanup(true);
        if (sess->socket) sess->socket->disconnectFromHost();
    }
    m_sessions.clear();
    if (isListening()) close();
}

void TlsReceiverServer::incomingConnection(qintptr socketDescriptor) {
    if (m_sessions.size() >= 4) {
        QTcpSocket rej;
        rej.setSocketDescriptor(socketDescriptor);
        rej.abort();
        return;
    }

    auto* sock = new QSslSocket(this);
    if (!sock->setSocketDescriptor(socketDescriptor)) {
        sock->deleteLater();
        return;
    }

    QSslConfiguration cfg = sock->sslConfiguration();
    cfg.setProtocol(QSsl::TlsV1_3OrLater);
    cfg.setLocalCertificate(m_cert);
    cfg.setPrivateKey(m_key);
    cfg.setPeerVerifyMode(QSslSocket::VerifyNone);
    sock->setSslConfiguration(cfg);

    auto sess = std::make_shared<ClientSession>();
    sess->socket = sock;
    m_sessions.insert(sock, sess);

    connect(sock, &QSslSocket::encrypted, this, &TlsReceiverServer::onSocketEncrypted);
    connect(sock, &QSslSocket::readyRead, this, &TlsReceiverServer::onReadyRead);
    connect(sock, &QSslSocket::disconnected, this, &TlsReceiverServer::onDisconnected);

    sock->startServerEncryption();
}

void TlsReceiverServer::onSocketEncrypted() {
    auto* sock = qobject_cast<QSslSocket*>(sender());
    if (!sock) return;
    emit logMessage(QStringLiteral("TLS 1.3 handshake completed with %1").arg(sock->peerAddress().toString()));
}

void TlsReceiverServer::onReadyRead() {
    auto* sock = qobject_cast<QSslSocket*>(sender());
    if (!sock || !m_sessions.contains(sock)) return;
    auto sess = m_sessions.value(sock);
    sess->rxBuffer.append(sock->readAll());

    while (true) {
        FrameHeader hdr;
        QByteArray payload;
        bool fatal = false;
        QString errMsg;
        if (!tryDecodeFrame(sess->rxBuffer, hdr, payload, fatal, errMsg)) {
            if (fatal) {
                emit logMessage(QStringLiteral("Protocol error: %1").arg(errMsg));
                sock->disconnectFromHost();
            }
            break;
        }
        handleFrame(*sess, hdr, payload);
    }
}

void TlsReceiverServer::onDisconnected() {
    auto* sock = qobject_cast<QSslSocket*>(sender());
    if (!sock) return;
    if (m_sessions.contains(sock)) {
        auto sess = m_sessions.take(sock);
        if (sess->writer) sess->writer->abortAndCleanup(true);
    }
    sock->deleteLater();
    emit clientDisconnected();
}

void TlsReceiverServer::sendJson(QSslSocket* sock, MessageType type, const QJsonObject& obj) {
    if (sock && sock->state() == QAbstractSocket::ConnectedState) {
        sock->write(encodeJsonFrame(type, obj));
    }
}

void TlsReceiverServer::respondToPairing(const QString& deviceId, bool accepted) {
    for (auto& sess : m_sessions) {
        if (sess->deviceId == deviceId && sess->pairingPending) {
            sess->pairingPending = false;
            if (accepted) {
                sess->authenticated = true;
                sendJson(sess->socket, MessageType::PairConfirm, {{QStringLiteral("accepted"), true}});
                emit clientAuthenticated(sess->deviceName, sess->deviceId);
            } else {
                sendJson(sess->socket, MessageType::PairReject, {{QStringLiteral("reason"), QStringLiteral("User rejected")}});
                sess->socket->disconnectFromHost();
            }
            return;
        }
    }
}

void TlsReceiverServer::handleFrame(ClientSession& sess, const FrameHeader& hdr, const QByteArray& payload) {
    if (hdr.type == MessageType::Ping) {
        sendJson(sess.socket, MessageType::Pong, {});
        return;
    }

    if (hdr.type == MessageType::Hello) {
        auto obj = QJsonDocument::fromJson(payload).object();
        sess.deviceId = obj.value(QStringLiteral("deviceId")).toString();
        sess.deviceName = obj.value(QStringLiteral("deviceName")).toString(QStringLiteral("Android Device"));
        sendJson(sess.socket, MessageType::HelloAck, {
            {QStringLiteral("protocolMajor"), kProtocolVersionMajor},
            {QStringLiteral("protocolMinor"), kProtocolVersionMinor},
            {QStringLiteral("serverName"), QSysInfo::machineHostName()}
        });
        return;
    }

    if (hdr.type == MessageType::PairRequest) {
        auto obj = QJsonDocument::fromJson(payload).object();
        QString nonce = obj.value(QStringLiteral("nonce")).toString();
        QByteArray certDigest = m_cert.digest(QCryptographicHash::Sha256);
        QByteArray sasInput = certDigest + nonce.toUtf8() + sess.deviceId.toUtf8();
        QByteArray sasHash = QCryptographicHash::hash(sasInput, QCryptographicHash::Sha256);
        quint32 codeNum = (static_cast<quint8>(sasHash[0]) << 16 | static_cast<quint8>(sasHash[1]) << 8 | static_cast<quint8>(sasHash[2])) % 1000000;
        sess.sasCode = QStringLiteral("%1").arg(codeNum, 6, 10, QLatin1Char('0'));

        if (!m_requirePairing) {
            sess.authenticated = true;
            sendJson(sess.socket, MessageType::PairConfirm, {{QStringLiteral("accepted"), true}, {QStringLiteral("sas"), sess.sasCode}});
            emit clientAuthenticated(sess.deviceName, sess.deviceId);
        } else {
            sess.pairingPending = true;
            sendJson(sess.socket, MessageType::PairChallenge, {{QStringLiteral("sas"), sess.sasCode}});
            emit pairingRequested(sess.deviceName, sess.deviceId, sess.sasCode);
        }
        return;
    }

    if (!sess.authenticated) {
        sendJson(sess.socket, MessageType::Error, {{QStringLiteral("message"), QStringLiteral("Unauthenticated")}});
        sess.socket->disconnectFromHost();
        return;
    }

    if (hdr.type == MessageType::Manifest) {
        auto obj = QJsonDocument::fromJson(payload).object();
        sess.jobId = obj.value(QStringLiteral("jobId")).toString();
        const auto files = obj.value(QStringLiteral("files")).toArray();
        if (files.size() > kMaxManifestEntries) {
            sendJson(sess.socket, MessageType::Error, {{QStringLiteral("message"), QStringLiteral("Manifest too large")}});
            return;
        }
        sess.stats = TransferStats{};
        sess.stats.totalFiles = files.size();
        for (const auto& f : files) {
            sess.stats.totalBytes += static_cast<uint64_t>(f.toObject().value(QStringLiteral("size")).toInteger());
        }
        sess.jobTimer.start();
        sendJson(sess.socket, MessageType::ManifestAck, {{QStringLiteral("jobId"), sess.jobId}, {QStringLiteral("accepted"), true}});
        emit transferStarted(sess.stats.totalFiles, sess.stats.totalBytes);
        emit transferProgress(sess.stats);
        return;
    }

    if (hdr.type == MessageType::FileStart) {
        auto obj = QJsonDocument::fromJson(payload).object();
        QString relPath = obj.value(QStringLiteral("relativePath")).toString();
        uint64_t size = static_cast<uint64_t>(obj.value(QStringLiteral("size")).toInteger());
        QString sha256 = obj.value(QStringLiteral("sha256")).toString();

        QString reason;
        QString destPath = PathValidator::safeResolveDestination(m_destRoot, relPath, &reason);
        if (destPath.isEmpty()) {
            sess.stats.failedFiles++;
            sendJson(sess.socket, MessageType::FileAck, {{QStringLiteral("relativePath"), relPath}, {QStringLiteral("ok"), false}, {QStringLiteral("error"), reason}});
            return;
        }

        if (QFileInfo::exists(destPath)) {
            if (m_conflictPolicy == ConflictPolicy::SkipExisting) {
                sess.stats.skippedFiles++;
                sendJson(sess.socket, MessageType::FileAck, {{QStringLiteral("relativePath"), relPath}, {QStringLiteral("ok"), true}, {QStringLiteral("skipped"), true}});
                return;
            } else if (m_conflictPolicy == ConflictPolicy::KeepBothRename) {
                destPath = PathValidator::uniqueDestinationPath(destPath);
            }
        }

        sess.writer = std::make_unique<SafeFileWriter>();
        if (!sess.writer->begin(destPath, size, sha256, true, &reason)) {
            sess.stats.failedFiles++;
            sendJson(sess.socket, MessageType::FileAck, {{QStringLiteral("relativePath"), relPath}, {QStringLiteral("ok"), false}, {QStringLiteral("error"), reason}});
            sess.writer.reset();
            return;
        }
        sess.activeRelPath = relPath;
        sess.activeFileSize = size;
        sess.stats.currentFile = relPath;
        sendJson(sess.socket, MessageType::ResumeAck, {{QStringLiteral("relativePath"), relPath}, {QStringLiteral("offset"), static_cast<qint64>(sess.writer->bytesWritten())}});
        emit fileProgress(relPath, sess.writer->bytesWritten(), size, false);
        emit transferProgress(sess.stats);
        return;
    }

    if (hdr.type == MessageType::FileChunk) {
        if (!sess.writer || payload.size() < 8) return;
        const auto* ptr = reinterpret_cast<const uchar*>(payload.constData());
        uint64_t offset = qFromBigEndian<uint64_t>(ptr);
        QByteArray data = payload.mid(8);
        QString err;
        if (!sess.writer->writeChunk(offset, data, &err)) {
            emit logMessage(QStringLiteral("Write error on %1: %2").arg(sess.activeRelPath, err));
            sess.writer->abortAndCleanup(true);
            sess.writer.reset();
            sess.stats.failedFiles++;
            sendJson(sess.socket, MessageType::FileAck, {{QStringLiteral("relativePath"), sess.activeRelPath}, {QStringLiteral("ok"), false}, {QStringLiteral("error"), err}});
            return;
        }
        sess.stats.receivedBytes += static_cast<uint64_t>(data.size());
        double secs = std::max(0.001, sess.jobTimer.elapsed() / 1000.0);
        sess.stats.averageMbps = (sess.stats.receivedBytes / (1024.0 * 1024.0)) / secs;
        sess.stats.currentMbps = sess.stats.averageMbps;
        emit fileProgress(sess.activeRelPath, sess.writer->bytesWritten(), sess.activeFileSize, false);
        emit transferProgress(sess.stats);
        return;
    }

    if (hdr.type == MessageType::FileEnd) {
        if (!sess.writer) return;
        ZeroByteClass zc = ZeroByteClass::NotZeroByte;
        QString err;
        bool ok = sess.writer->finalize(zc, &err);
        sess.writer.reset();
        if (ok) {
            sess.stats.verifiedFiles++;
        } else {
            sess.stats.failedFiles++;
        }
        sendJson(sess.socket, MessageType::FileAck, {
            {QStringLiteral("relativePath"), sess.activeRelPath},
            {QStringLiteral("ok"), ok},
            {QStringLiteral("zeroByteClass"), zeroByteClassToString(zc)},
            {QStringLiteral("error"), err}
        });
        emit fileProgress(sess.activeRelPath, sess.activeFileSize, sess.activeFileSize, ok);
        emit transferProgress(sess.stats);
        return;
    }

    if (hdr.type == MessageType::JobComplete) {
        emit jobCompleted(sess.stats);
    }
}

} // namespace directdrop
