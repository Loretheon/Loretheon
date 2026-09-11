#include "EditCandidateView.h"

#include "../../text/TextEdit.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QTextCursor>
#include <QTextEdit>

EditCandidateView::EditCandidateView(TextEdit *editor, QObject *parent)
    : QObject(parent), m_editor(nullptr) {
  setEditor(editor);
}

void EditCandidateView::setEditor(TextEdit *editor) {
  if (m_editor == editor) {
    return;
  }

  if (m_editor) {
    m_editor->viewport()->removeEventFilter(this);
  }

  m_editor = editor;

  if (m_editor) {
    m_editor->viewport()->installEventFilter(this);
  }

  clear();
}

void EditCandidateView::showCandidates(const QVector<EditMatch> &candidates) {
  clear();

  if (!m_editor || candidates.isEmpty()) {
    return;
  }

  m_candidates = candidates;

  QList<QTextEdit::ExtraSelection> selections;
  selections.reserve(m_candidates.size());

  const QPalette &palette = m_editor->palette();
  const QBrush highlight = palette.brush(QPalette::Active, QPalette::Highlight);
  const QBrush highlightedText =
      palette.brush(QPalette::Active, QPalette::HighlightedText);

  for (const EditMatch &candidate : m_candidates) {
    if (!candidate.isValid()) {
      continue;
    }

    QTextCursor cursor(m_editor->document());
    cursor.setPosition(candidate.start);
    cursor.setPosition(candidate.end, QTextCursor::KeepAnchor);

    QTextEdit::ExtraSelection selection;
    selection.cursor = cursor;
    selection.format.setBackground(highlight);
    selection.format.setForeground(highlightedText);

    selections.append(selection);
  }

  m_editor->setExtraSelections(selections);
}

void EditCandidateView::clear() {
  m_candidates.clear();

  if (m_editor) {
    m_editor->setExtraSelections({});
  }
}

bool EditCandidateView::eventFilter(QObject *watched, QEvent *event) {
  if (!m_editor || watched != m_editor->viewport()) {
    return QObject::eventFilter(watched, event);
  }

  if (event->type() != QEvent::MouseButtonPress) {
    return QObject::eventFilter(watched, event);
  }

  const auto *mouseEvent = static_cast<QMouseEvent *>(event);

  if (!mouseEvent) {
    return QObject::eventFilter(watched, event);
  }

  const int index = candidateAtPosition(mouseEvent->position().toPoint());

  if (index < 0) {
    return QObject::eventFilter(watched, event);
  }

  emit candidateSelected(index);
  return true;
}

int EditCandidateView::candidateAtPosition(const QPoint &position) const {
  if (!m_editor) {
    return -1;
  }

  const QTextCursor cursor = m_editor->cursorForPosition(position);

  const int documentPosition = cursor.position();

  for (int index = 0; index < m_candidates.size(); ++index) {
    const EditMatch &candidate = m_candidates.at(index);

    if (!candidate.isValid()) {
      continue;
    }

    if (documentPosition >= candidate.start &&
        documentPosition < candidate.end) {
      return index;
    }
  }

  return -1;
}