#include "../../include/overseer/OverseerOverviewEditor.h"

#include "../../include/file/FileSystemView.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

namespace {

// Extract the notes paths from our custom MIME payload. The format is one
// absolute path per line, UTF-8, no trailing newline.
QStringList pathsFromNotesPayload(const QMimeData *mime) {
  QStringList paths;

  if (!mime) {
    return paths;
  }

  const QByteArray raw =
      mime->data(QString::fromLatin1(FileSystemView::notesPathMimeType()));

  if (raw.isEmpty()) {
    return paths;
  }

  const QString text = QString::fromUtf8(raw);

  const QStringList lines = text.split(QChar('\n'), Qt::SkipEmptyParts);

  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();

    if (trimmed.isEmpty()) {
      continue;
    }

    const QFileInfo info(trimmed);

    if (!info.exists() || info.isDir()) {
      continue;
    }

    paths.append(info.absoluteFilePath());
  }

  return paths;
}

} // namespace

OverseerOverviewEditor::OverseerOverviewEditor(QWidget *parent)
    : QPlainTextEdit(parent) {
  setAcceptDrops(true);
}

void OverseerOverviewEditor::dragEnterEvent(QDragEnterEvent *event) {
  if (!event) {
    return;
  }

  const QMimeData *mime = event->mimeData();

  if (mime && mime->hasFormat(
                  QString::fromLatin1(FileSystemView::notesPathMimeType()))) {
    event->acceptProposedAction();
    return;
  }

  // Everything else -- including external file drops as text/uri-list --
  // is rejected. External drops are deliberately not accepted here.
  event->ignore();
}

void OverseerOverviewEditor::dragMoveEvent(QDragMoveEvent *event) {
  if (!event) {
    return;
  }

  const QMimeData *mime = event->mimeData();

  if (mime && mime->hasFormat(
                  QString::fromLatin1(FileSystemView::notesPathMimeType()))) {
    event->acceptProposedAction();
    return;
  }

  event->ignore();
}

void OverseerOverviewEditor::dropEvent(QDropEvent *event) {
  if (!event) {
    return;
  }

  const QStringList paths = pathsFromNotesPayload(event->mimeData());

  if (paths.isEmpty()) {
    event->ignore();
    return;
  }

  event->acceptProposedAction();

  emit filesDropped(paths);
}