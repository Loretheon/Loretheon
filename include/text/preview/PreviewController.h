#pragma once

#include "PreviewPane.h"

#include <QObject>
#include <QString>
#include <QVector>

class EditSession;
class TextEdit;
class TextDocument;
class PendingEdit;

class PreviewController : public QObject {
  Q_OBJECT

public:
  PreviewController(TextEdit *editor, EditSession *session,
                    PreviewPane *pane, QObject *parent = nullptr);

  void setEditor(TextEdit *editor);
  void setSession(EditSession *session);

  // Enable / disable preview mode. Enabling immediately rebuilds the shadow
  // string from the current editor text plus accepted+completed pending edits.
  void setPreviewActive(bool active);
  bool isPreviewActive() const { return m_active; }

  // Force a rebuild of the shadow string and highlights.
  void refresh();

private slots:
  void onPendingEditsChanged();
  void onPendingEditUpdated(const PendingEdit &edit);
  void onPendingEditFinished(const PendingEdit &edit);

private:
  QString buildShadowText(QVector<PreviewPane::Highlight> &highlights) const;

  TextEdit *m_editor = nullptr;
  EditSession *m_session = nullptr;
  PreviewPane *m_pane = nullptr;

  bool m_active = false;
};