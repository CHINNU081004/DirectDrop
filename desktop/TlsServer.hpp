#pragma once

#include "Protocol.hpp"
#include "SafeFileWriter.hpp"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <memory>

namespace directdrop {

struct TransferStats {
    int totalFiles = 0;
    int verifiedFiles = 0;
    int failedFiles = 0;
    int skippedFiles = 0;
    uint64_t totalBytes = 0;
    uint64_t receivedBytes = 0;
    double currentMbps = 0.0;
    double averageMbps = 0.0;
    QString currentFile;
};

class TlsReceiverServer : public QTcpServer {
    Q_OBJECT
public:
    explicit TlsReceiverServer(QObject* parent = nullptr);
    ~TlsReceiverServer() override;

    bool startListening(const QHostAddress& bindAddr, quint16 port = 47291, QString* outError = nullptr);
    void stopListening();

    void setDestinationRoot(const QString& path) { m_destRoot = path; }
    QString destinationRoot() const { return m_destRoot; }
    void setConflictPolicy(ConflictPolicy policy) { m_conflictPolicy = policy; }
    void setRequirePairingConfirmation(bool req) { m_requirePairing = req; }
    void respondToPairing(const QString& deviceId, bool accepted);

signals:
    void logMessage(const QString& msg);
    void pairingRequested(const QString& deviceName, const QString& deviceId, const QString& sasCode);
    void clientAuthenticated(const QString& deviceName, const QString& deviceId);
    void transferStarted(int totalFiles, uint64_t totalBytes);
    void fileProgress(const QString& relPath, uint64_t receivedBytes, uint64_t totalBytes, bool verified);
    void transferProgress(const TransferStats& stats);
    void jobCompleted(const TransferStats& stats);
    void clientDisconnected();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onSocketEncrypted();
    void onReadyRead();
    void onDisconnected();

private:
    struct ClientSession {
        QSslSocket* socket = nullptr;
        QByteArray rxBuffer;
        bool authenticated = false;
        bool pairingPending = false;
        QString deviceId;
        QString deviceName;
        QString sasCode;
        QString jobId;
        std::unique_ptr<SafeFileWriter> writer;
        QString activeRelPath;
        uint64_t activeFileSize = 0;
        TransferStats stats;
        QElapsedTimer jobTimer;
    };

    bool ensureServerIdentity();
    void handleFrame(ClientSession& sess, const FrameHeader& hdr, const QByteArray& payload);
    void sendJson(QSslSocket* sock, MessageType type, const QJsonObject& obj);

    QSslCertificate m_cert;
    QSslKey m_key;
    QString m_destRoot;
    ConflictPolicy m_conflictPolicy = ConflictPolicy::KeepBothRename;
    bool m_requirePairing = true;
    QHash<QSslSocket*, std::shared_ptr<ClientSession>> m_sessions;
};

} // namespace directdrop
