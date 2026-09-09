#include "EditCandidateView.h"

#include "../../text/TextEdit.h"

#include <QEvent>
#include <QMouseEvent>
#include <QTextCursor>
#include <QTextEdit>

EditCandidateView::EditCandidateView(TextEdit *editor, QObject *parent)
    : QObject(parent), m_editor(editor) {
  setEditor(editor);
}

void EditCandidateView::setEditor(TextEdit *editor) {
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

  if (!m_editor) {
    return;
  }

  m_candidates = candidates;

  QList<QTextEdit::ExtraSelection> selections;

  for (const EditMatch &match : m_candidates) {
    if (!match.isValid()) {
      continue;
    }

    QTextEdit::ExtraSelection selection;

    QTextCursor cursor(m_editor->document());

    cursor.setPosition(match.start);
    cursor.setPosition(match.end, QTextCursor::KeepAnchor);

    selection.cursor = cursor;

    /*
     * Use Qt's normal selection palette rather than introducing a
     * custom application colour scheme.
     */
    selection.format.setBackground(
        m_editor->palette().brush(QPalette::Active, QPalette::Highlight));

    selection.format.setForeground(
        m_editor->palette().brush(QPalette::Active, QPalette::HighlightedText));

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
  if (watched != (m_editor ? m_editor->viewport() : nullptr)) {
    return QObject::eventFilter(watched, event);
  }

  if (event->type() == QEvent::MouseButtonPress) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);

    const int index = candidateAtPosition(mouseEvent->position().toPoint());

    if (index >= 0) {
      emit candidateSelected(index);
      return true;
    }
  }

  return QObject::eventFilter(watched, event);
}

int EditCandidateView::candidateAtPosition(const QPoint &position) const {
  if (!m_editor) {
    return -1;
  }

  QTextCursor cursor = m_editor->cursorForPosition(position);

  const int positionInDocument = cursor.position();

  for (int i = 0; i < m_candidates.size(); ++i) {
    const EditMatch &candidate = m_candidates[i];

    if (positionInDocument >= candidate.start &&
        positionInDocument < candidate.end) {
      return i;
    }
  }

  return -1;
}