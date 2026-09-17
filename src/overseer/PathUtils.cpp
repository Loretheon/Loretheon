#include "../../include/overseer/PathUtils.h"

#include <QDir>
#include <QFileInfo>

namespace PathUtils {

QString toRelative(const QString &absolutePath, const QString &rootPath) {
  if (absolutePath.isEmpty() || rootPath.isEmpty())
    return {};

  const QFileInfo absInfo(absolutePath);
  const QFileInfo rootInfo(rootPath);

  const QString abs = absInfo.absoluteFilePath();
  const QString root = rootInfo.absoluteFilePath();

  if (!abs.startsWith(root))
    return {};

  return QDir(root).relativeFilePath(abs);
}

QString toAbsolute(const QString &relativePath, const QString &rootPath) {
  if (relativePath.isEmpty() || rootPath.isEmpty())
    return {};

  return QDir(rootPath).filePath(relativePath);
}

bool isUnder(const QString &absolutePath, const QString &rootPath) {
  if (absolutePath.isEmpty() || rootPath.isEmpty())
    return false;

  const QString abs = QFileInfo(absolutePath).absoluteFilePath();
  const QString root = QFileInfo(rootPath).absoluteFilePath();

  if (abs == root)
    return true;

  return abs.startsWith(root + QDir::separator());
}

} // namespace PathUtils