#include "OverseerOverviewEditor.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

OverseerOverviewEditor::OverseerOverviewEditor(QWidget *parent)
    : QPlainTextEdit(parent) {
  setAcceptDrops(true);
}

void OverseerOverviewEditor::dragEnterEvent(QDragEnterEvent *event) {
  if (!event) {
    return;
  }

  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
    return;
  }

  QPlainTextEdit::dragEnterEvent(event);
}

void OverseerOverviewEditor::dragMoveEvent(QDragMoveEvent *event) {
  if (!event) {
    return;
  }

  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
    return;
  }

  QPlainTextEdit::dragMoveEvent(event);
}

void OverseerOverviewEditor::dropEvent(QDropEvent *event) {
  if (!event) {
    return;
  }

  const QMimeData *mime = event->mimeData();

  if (!mime || !mime->hasUrls()) {
    QPlainTextEdit::dropEvent(event);
    return;
  }

  QStringList paths;

  for (const QUrl &url : mime->urls()) {
    if (!url.isLocalFile()) {
      continue;
    }

    const QString local = url.toLocalFile();

    if (local.isEmpty()) {
      continue;
    }

    const QFileInfo info(local);

    if (!info.exists()) {
      continue;
    }

    paths.append(info.absoluteFilePath());
  }

  if (paths.isEmpty()) {
    event->ignore();
    return;
  }

  event->acceptProposedAction();

  emit filesDropped(paths);
}