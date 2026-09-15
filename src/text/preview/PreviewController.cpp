#include "../../../include/text/preview/PreviewController.h"

#include "../../../include/ai/edit/EditSession.h"
#include "../../../include/ai/edit/PendingEdit.h"
#include "../../../include/text/TextEdit.h"
#include "../../../include/text/model/TextDocument.h"

#include <QColor>

#include <algorithm>

PreviewController::PreviewController(TextEdit *editor, EditSession *session,
                                     PreviewPane *pane, QObject *parent)
    : QObject(parent), m_editor(editor), m_session(session), m_pane(pane) {
  if (m_session) {
    connect(m_session, &EditSession::pendingEditsChanged, this,
            &PreviewController::onPendingEditsChanged);

    connect(m_session, &EditSession::pendingEditUpdated, this,
            &PreviewController::onPendingEditUpdated);

    connect(m_session, &EditSession::pendingEditFinished, this,
            &PreviewController::onPendingEditFinished);
  }
}

void PreviewController::setEditor(TextEdit *editor) {
  m_editor = editor;

  if (m_active) {
    refresh();
  }
}

void PreviewController::setSession(EditSession *session) {
  if (m_session == session) {
    return;
  }

  if (m_session) {
    disconnect(m_session, nullptr, this, nullptr);
  }

  m_session = session;

  if (m_session) {
    connect(m_session, &EditSession::pendingEditsChanged, this,
            &PreviewController::onPendingEditsChanged);

    connect(m_session, &EditSession::pendingEditUpdated, this,
            &PreviewController::onPendingEditUpdated);

    connect(m_session, &EditSession::pendingEditFinished, this,
            &PreviewController::onPendingEditFinished);
  }

  if (m_active) {
    refresh();
  }
}

void PreviewController::setPreviewActive(bool active) {
  if (m_active == active) {
    return;
  }

  m_active = active;

  if (!m_active) {
    if (m_pane) {
      m_pane->clearAll();
    }
    return;
  }

  refresh();
}

void PreviewController::refresh() {
  if (!m_active || !m_pane) {
    return;
  }

  if (!m_editor) {
    m_pane->clearAll();
    return;
  }

  QVector<PreviewPane::Highlight> highlights;

  const QString shadow = buildShadowText(highlights);

  m_pane->setShadowText(shadow);
  m_pane->setHighlights(highlights);
}

void PreviewController::onPendingEditsChanged() { refresh(); }

void PreviewController::onPendingEditUpdated(const PendingEdit &) { refresh(); }

void PreviewController::onPendingEditFinished(const PendingEdit &) {
  refresh();
}

QString PreviewController::buildShadowText(
    QVector<PreviewPane::Highlight> &highlights) const {
  highlights.clear();

  if (!m_editor) {
    return {};
  }

  const QString original = m_editor->toPlainText();

  if (!m_session) {
    return original;
  }

  // Collect completed and accepted edits. Apply them in descending offset
  // order so earlier edits' offsets stay valid.
  struct Apply {
    const PendingEdit *edit = nullptr;
  };

  QVector<const PendingEdit *> selected;

  for (const PendingEdit &edit : m_session->pendingEdits()) {
    if (!edit.completed) {
      continue;
    }

    if (!edit.accepted) {
      continue;
    }

    if (!edit.match.isValid()) {
      continue;
    }

    selected.append(&edit);
  }

  std::sort(selected.begin(), selected.end(),
            [](const PendingEdit *a, const PendingEdit *b) {
              return a->match.start > b->match.start;
            });

  QString shadow = original;

  // Green for accepted inserts and replaces, red for deletes.
  QColor insertColor(100, 200, 120, 90);
  QColor replaceColor(200, 190, 100, 90);
  QColor deleteColor(210, 100, 100, 90);

  // Track offsets in the shadow string. Because we apply from the end
  // backwards, positions ahead of the splice remain stable; positions
  // behind shift by the delta. We record highlight ranges in the final
  // string directly.
  for (const PendingEdit *edit : selected) {
    const int start = edit->match.start;
    const int end = edit->match.end;

    if (start < 0 || end < start || end > shadow.size()) {
      continue;
    }

    const QString replacement = edit->generatedText;

    switch (edit->command.operation) {
    case EditCommand::Operation::Insert: {
      shadow.insert(start, replacement);

      PreviewPane::Highlight highlight;
      highlight.start = start;
      highlight.end = start + replacement.size();
      highlight.color = insertColor;
      highlights.append(highlight);
      break;
    }

    case EditCommand::Operation::Replace:
    case EditCommand::Operation::ReplaceScope: {
      shadow.replace(start, end - start, replacement);

      PreviewPane::Highlight highlight;
      highlight.start = start;
      highlight.end = start + replacement.size();
      highlight.color = replaceColor;
      highlights.append(highlight);
      break;
    }

    case EditCommand::Operation::Delete: {
      PreviewPane::Highlight highlight;
      highlight.start = start;
      highlight.end = end;
      highlight.color = deleteColor;
      highlight.strikethrough = true;
      highlights.append(highlight);

      shadow.remove(start, end - start);
      break;
    }

    case EditCommand::Operation::Unknown:
      break;
    }
  }

  return shadow;
}