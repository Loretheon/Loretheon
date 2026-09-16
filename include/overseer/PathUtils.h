#pragma once

#include <QString>

namespace PathUtils {

// Convert an absolute path to a notes-root-relative path. Returns empty
// if the path is not under the root.
QString toRelative(const QString &absolutePath, const QString &rootPath);

// Resolve a notes-root-relative path to an absolute one.
QString toAbsolute(const QString &relativePath, const QString &rootPath);

// True if the given path is under the root (inclusive).
bool isUnder(const QString &absolutePath, const QString &rootPath);

}