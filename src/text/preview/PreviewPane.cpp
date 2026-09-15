#include "PreviewPane.h"

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>

PreviewPane::PreviewPane(QWidget *parent) : QPlainTextEdit(parent) {
  setReadOnly(true);
  setLineWrapMode(QPlainTextEdit::WidgetWidth);
  setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  setFrameShape(QFrame::NoFrame);
  setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  setObjectName(QStringLiteral("previewPane"));
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void PreviewPane::setShadowText(const QString &text) {
  if (toPlainText() == text) {
    return;
  }

  QPlainTextEdit::setPlainText(text);
}

void PreviewPane::setHighlights(const QVector<Highlight> &highlights) {
  m_highlights = highlights;

  QList<QTextEdit::ExtraSelection> selections;
  selections.reserve(highlights.size());

  const int documentLength = document()->characterCount();

  for (const Highlight &highlight : highlights) {
    if (highlight.start < 0 || highlight.end < highlight.start) {
      continue;
    }

    const int start = qBound(0, highlight.start, documentLength);
    const int end = qBound(start, highlight.end, documentLength);

    if (end <= start) {
      continue;
    }

    QTextCursor cursor(document());
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);

    QTextEdit::ExtraSelection selection;
    selection.cursor = cursor;

    QTextCharFormat format;
    format.setBackground(highlight.color);

    if (highlight.strikethrough) {
      format.setFontStrikeOut(true);
    }

    selection.format = format;
    selections.append(selection);
  }

  setExtraSelections(selections);
}

void PreviewPane::clearAll() {
  clear();
  setExtraSelections({});
  m_highlights.clear();
}