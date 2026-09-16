#include "../../include/overseer/OverseerOverviewEditor.h"

#include "../../include/file/FileSystemView.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTimer>
#include <QUrl>

namespace {

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

// Extract the path from a "- /abs/path" line, or an empty string.
QString pathFromOverviewLine(const QString &line) {
  static const QRegularExpression re(
      QStringLiteral("^\\s*-\\s+(\\S.*?)\\s*$"));

  const auto match = re.match(line);

  if (!match.hasMatch()) {
    return {};
  }

  return match.captured(1).trimmed();
}

} // namespace

OverseerOverviewEditor::OverseerOverviewEditor(QWidget *parent)
    : QPlainTextEdit(parent) {
  setAcceptDrops(true);

  // Debounce revalidation while the user types.
  auto *debounce = new QTimer(this);
  debounce->setSingleShot(true);
  debounce->setInterval(250);

  connect(this, &QPlainTextEdit::textChanged, debounce,
          qOverload<>(&QTimer::start));

  connect(debounce, &QTimer::timeout, this,
          &OverseerOverviewEditor::revalidateReferences);

  revalidateReferences();
}

void OverseerOverviewEditor::revalidateReferences() {
  QStringList missing;

  for (QTextBlock block = document()->firstBlock(); block.isValid();
       block = block.next()) {
    const QString line = block.text();
    const QString path = pathFromOverviewLine(line);

    if (path.isEmpty()) {
      continue;
    }

    if (!QFileInfo::exists(path)) {
      missing.append(path);
    }
  }

  if (missing == m_missing) {
    applyMissingHighlight();
    return;
  }

  m_missing = missing;
  applyMissingHighlight();
  emit missingReferencesChanged(m_missing);
}

void OverseerOverviewEditor::applyMissingHighlight() {
  QList<QTextEdit::ExtraSelection> selections;

  const QTextCharFormat strike = [] {
    QTextCharFormat format;
    format.setFontStrikeOut(true);
    format.setForeground(QColor(210, 100, 100));
    return format;
  }();

  for (QTextBlock block = document()->firstBlock(); block.isValid();
       block = block.next()) {
    const QString line = block.text();
    const QString path = pathFromOverviewLine(line);

    if (path.isEmpty() || !m_missing.contains(path)) {
      continue;
    }

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QTextEdit::ExtraSelection selection;
    selection.cursor = cursor;
    selection.format = strike;
    selections.append(selection);
  }

  setExtraSelections(selections);
}

int OverseerOverviewEditor::removeMissingReferences() {
  if (m_missing.isEmpty()) {
    return 0;
  }

  QList<int> blockNumbers;

  for (QTextBlock block = document()->firstBlock(); block.isValid();
       block = block.next()) {
    const QString line = block.text();
    const QString path = pathFromOverviewLine(line);

    if (path.isEmpty() || !m_missing.contains(path)) {
      continue;
    }

    blockNumbers.append(block.blockNumber());
       }

  QTextCursor cursor(document());
  cursor.beginEditBlock();

  // Reverse order so earlier block numbers remain valid.
  for (int i = blockNumbers.size() - 1; i >= 0; --i) {
    const int n = blockNumbers.at(i);
    QTextBlock block = document()->findBlockByNumber(n);

    if (!block.isValid()) {
      continue;
    }

    QTextCursor blockCursor(block);
    blockCursor.select(QTextCursor::BlockUnderCursor);
    blockCursor.removeSelectedText();
    blockCursor.deleteChar();
  }

  cursor.endEditBlock();

  revalidateReferences();
  return blockNumbers.size();
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