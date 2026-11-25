#pragma once

#include <QString>

namespace directdrop {

class PathValidator {
public:
    /// Validates a relative path from a remote peer.
    /// Rejects empty paths, absolute paths, '..', backslashes, NUL bytes,
    /// control characters, Windows drive letters, and paths exceeding 4096 chars.
    static bool isValidRelativePath(const QString& relPath, QString* outReason = nullptr);

    /// Resolves a validated relative path safely under destRoot.
    /// Returns empty string if validation or canonicalization escapes destRoot.
    static QString safeResolveDestination(const QString& destRoot, const QString& relPath, QString* outReason = nullptr);

    /// Generates a unique non-conflicting path (e.g., "photo (1).jpg") if target exists.
    static QString uniqueDestinationPath(const QString& targetPath);
};

} // namespace directdrop
