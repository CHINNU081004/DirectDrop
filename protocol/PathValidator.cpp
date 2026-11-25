#include "PathValidator.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace directdrop {

bool PathValidator::isValidRelativePath(const QString& relPath, QString* outReason) {
    if (relPath.isEmpty()) {
        if (outReason) *outReason = QStringLiteral("Empty path");
        return false;
    }
    if (relPath.size() > 4096) {
        if (outReason) *outReason = QStringLiteral("Path exceeds 4096 characters");
        return false;
    }
    if (relPath.startsWith(QLatin1Char('/')) || relPath.startsWith(QLatin1Char('\\'))) {
        if (outReason) *outReason = QStringLiteral("Absolute path not allowed");
        return false;
    }
    if (relPath.contains(QLatin1Char('\\'))) {
        if (outReason) *outReason = QStringLiteral("Backslash not allowed in relative path");
        return false;
    }
    if (relPath.size() >= 2 && relPath[1] == QLatin1Char(':')) {
        if (outReason) *outReason = QStringLiteral("Drive prefix not allowed");
        return false;
    }
    for (const QChar ch : relPath) {
        if (ch.unicode() < 0x20 || ch.unicode() == 0x7F) {
            if (outReason) *outReason = QStringLiteral("Control character or NUL byte not allowed");
            return false;
        }
    }

    const QStringList segments = relPath.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    for (const QString& seg : segments) {
        if (seg.isEmpty() || seg == QStringLiteral(".") || seg == QStringLiteral("..")) {
            if (outReason) *outReason = QStringLiteral("Invalid path segment: '%1'").arg(seg);
            return false;
        }
    }
    return true;
}

QString PathValidator::safeResolveDestination(const QString& destRoot, const QString& relPath, QString* outReason) {
    if (!isValidRelativePath(relPath, outReason)) {
        return {};
    }
    QDir rootDir(destRoot);
    QString cleanRoot = QDir::cleanPath(rootDir.absolutePath());
    QString candidate = QDir::cleanPath(cleanRoot + QLatin1Char('/') + relPath);

    if (!candidate.startsWith(cleanRoot + QLatin1Char('/'))) {
        if (outReason) *outReason = QStringLiteral("Resolved path escapes destination root");
        return {};
    }
    return candidate;
}

QString PathValidator::uniqueDestinationPath(const QString& targetPath) {
    if (!QFileInfo::exists(targetPath)) {
        return targetPath;
    }
    QFileInfo info(targetPath);
    QString dir = info.absolutePath();
    QString base = info.completeBaseName();
    QString ext = info.suffix();
    for (int i = 1; i < 100000; ++i) {
        QString candidate = ext.isEmpty()
            ? QStringLiteral("%1/%2 (%3)").arg(dir, base).arg(i)
            : QStringLiteral("%1/%2 (%3).%4").arg(dir, base).arg(i).arg(ext);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

} // namespace directdrop
