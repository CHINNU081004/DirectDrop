#include "PathValidator.hpp"
#include "Protocol.hpp"
#include "SafeFileWriter.hpp"
#include "TlsServer.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace directdrop;

class DirectDropTests : public QObject {
    Q_OBJECT
private slots:
    void testFrameEncodeDecode() {
        QJsonObject obj{{QStringLiteral("hello"), QStringLiteral("world")}};
        QByteArray frame = encodeJsonFrame(MessageType::Hello, obj);
        FrameHeader hdr;
        QByteArray payload;
        bool fatal = false;
        QString err;
        QVERIFY(tryDecodeFrame(frame, hdr, payload, fatal, err));
        QVERIFY(!fatal);
        QCOMPARE(hdr.type, MessageType::Hello);
        QCOMPARE(hdr.versionMajor, kProtocolVersionMajor);
        QVERIFY(frame.isEmpty());
    }

    void testInvalidMagicRejected() {
        QByteArray bad("BADMAGIC12345678");
        FrameHeader hdr;
        QByteArray payload;
        bool fatal = false;
        QString err;
        QVERIFY(!tryDecodeFrame(bad, hdr, payload, fatal, err));
        QVERIFY(fatal);
    }

    void testPathTraversalRejected() {
        QString reason;
        QVERIFY(!PathValidator::isValidRelativePath(QStringLiteral("../secret.txt"), &reason));
        QVERIFY(!PathValidator::isValidRelativePath(QStringLiteral("DCIM/../../etc/passwd"), &reason));
        QVERIFY(!PathValidator::isValidRelativePath(QStringLiteral("/etc/passwd"), &reason));
        QVERIFY(!PathValidator::isValidRelativePath(QStringLiteral("C:\\Windows\\system32"), &reason));
        QVERIFY(!PathValidator::isValidRelativePath(QStringLiteral("DCIM\\Camera\\a.jpg"), &reason));
        QVERIFY(!PathValidator::isValidRelativePath(QStringLiteral(""), &reason));

        QTemporaryDir tmp;
        QVERIFY(PathValidator::safeResolveDestination(tmp.path(), QStringLiteral("../escape.txt")).isEmpty());
        QVERIFY(!PathValidator::safeResolveDestination(tmp.path(), QStringLiteral("DCIM/Camera/IMG 001 (üñîçødé).jpg")).isEmpty());
    }

    void testLegitimateEmptyAndHashMismatch() {
        QTemporaryDir tmp;
        QString emptyPath = tmp.path() + QStringLiteral("/empty.txt");
        QString emptyHash = QString::fromLatin1(QCryptographicHash::hash(QByteArray(), QCryptographicHash::Sha256).toHex());

        SafeFileWriter w1;
        QString err;
        QVERIFY(w1.begin(emptyPath, 0, emptyHash, false, &err));
        ZeroByteClass zc = ZeroByteClass::NotZeroByte;
        QVERIFY(w1.finalize(zc, &err));
        QCOMPARE(zc, ZeroByteClass::LegitimateEmpty);
        QVERIFY(QFile::exists(emptyPath));

        // Unexpected zero-byte for non-empty file
        QString nonEmptyPath = tmp.path() + QStringLiteral("/nonempty.bin");
        SafeFileWriter w2;
        QVERIFY(w2.begin(nonEmptyPath, 100, emptyHash, false, &err));
        QVERIFY(!w2.finalize(zc, &err));
        QCOMPARE(zc, ZeroByteClass::UnexpectedZero);
        QVERIFY(!QFile::exists(nonEmptyPath));
    }

    void testChunkResumeAndIntegrity() {
        QTemporaryDir tmp;
        QByteArray fullData("DirectDrop-Chunk-1-Data|DirectDrop-Chunk-2-Data");
        QString expectedHash = QString::fromLatin1(QCryptographicHash::hash(fullData, QCryptographicHash::Sha256).toHex());
        QString destPath = tmp.path() + QStringLiteral("/resumed.bin");

        // Phase 1: write first half and abort keeping partial
        {
            SafeFileWriter w;
            QString err;
            QVERIFY(w.begin(destPath, static_cast<uint64_t>(fullData.size()), expectedHash, true, &err));
            QVERIFY(w.writeChunk(0, fullData.left(23), &err));
            w.abortAndCleanup(true);
            QVERIFY(QFile::exists(destPath + QStringLiteral(".ddpart")));
        }

        // Phase 2: resume from offset 23
        {
            SafeFileWriter w;
            QString err;
            QVERIFY(w.begin(destPath, static_cast<uint64_t>(fullData.size()), expectedHash, true, &err));
            QCOMPARE(w.bytesWritten(), static_cast<uint64_t>(23));
            QVERIFY(w.writeChunk(23, fullData.mid(23), &err));
            ZeroByteClass zc;
            QVERIFY(w.finalize(zc, &err));
            QCOMPARE(zc, ZeroByteClass::NotZeroByte);
            QVERIFY(QFile::exists(destPath));
        }
    }

    void testEndToEndTlsTransfer() {
        QTemporaryDir tmp;
        TlsReceiverServer server;
        server.setDestinationRoot(tmp.path());
        server.setRequirePairingConfirmation(false);
        QString err;
        QVERIFY2(server.startListening(QHostAddress::LocalHost, 0, &err), qPrintable(err));

        QSslSocket client;
        QSslConfiguration cfg = client.sslConfiguration();
        cfg.setPeerVerifyMode(QSslSocket::VerifyNone);
        client.setSslConfiguration(cfg);
        QSignalSpy encryptedSpy(&client, &QSslSocket::encrypted);
        client.connectToHostEncrypted(QStringLiteral("127.0.0.1"), server.serverPort());
        QVERIFY(encryptedSpy.wait(5000));

        QByteArray fileData(128 * 1024, 'D');
        QString sha = QString::fromLatin1(QCryptographicHash::hash(fileData, QCryptographicHash::Sha256).toHex());

        client.write(encodeJsonFrame(MessageType::Hello, {{QStringLiteral("deviceId"), QStringLiteral("test-phone")}, {QStringLiteral("deviceName"), QStringLiteral("Realme P3 Pro")}}));
        client.write(encodeJsonFrame(MessageType::PairRequest, {{QStringLiteral("nonce"), QStringLiteral("abc123")}}));
        client.write(encodeJsonFrame(MessageType::Manifest, {
            {QStringLiteral("jobId"), QStringLiteral("job-1")},
            {QStringLiteral("files"), QJsonArray{QJsonObject{{QStringLiteral("relativePath"), QStringLiteral("DCIM/Camera/test.bin")}, {QStringLiteral("size"), fileData.size()}}}}
        }));
        client.write(encodeJsonFrame(MessageType::FileStart, {
            {QStringLiteral("relativePath"), QStringLiteral("DCIM/Camera/test.bin")},
            {QStringLiteral("size"), fileData.size()},
            {QStringLiteral("sha256"), sha}
        }));

        QByteArray chunkPayload(8 + fileData.size(), '\0');
        qToBigEndian<uint64_t>(0, reinterpret_cast<uchar*>(chunkPayload.data()));
        std::memcpy(chunkPayload.data() + 8, fileData.constData(), static_cast<size_t>(fileData.size()));
        client.write(encodeFrame(MessageType::FileChunk, chunkPayload));
        client.write(encodeJsonFrame(MessageType::FileEnd, {}));
        client.write(encodeJsonFrame(MessageType::JobComplete, {}));
        client.flush();

        QSignalSpy spy(&server, &TlsReceiverServer::jobCompleted);
        QVERIFY(spy.wait(5000));

        QString receivedFile = tmp.path() + QStringLiteral("/DCIM/Camera/test.bin");
        QVERIFY(QFile::exists(receivedFile));
        QCOMPARE(QFileInfo(receivedFile).size(), fileData.size());
    }
};

QTEST_MAIN(DirectDropTests)
#include "test_protocol_and_files.moc"
