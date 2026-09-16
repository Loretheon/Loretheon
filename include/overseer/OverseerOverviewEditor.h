#pragma once

#include <QPlainTextEdit>
#include <QStringList>

class OverseerOverviewEditor : public QPlainTextEdit {
  Q_OBJECT

public:
  explicit OverseerOverviewEditor(QWidget *parent = nullptr);

  // Re-scan the current text for "- /abs/path" lines whose files no
  // longer exist. Missing lines are struck through in the visible text
  // and their paths are returned.
  QStringList missingReferences() const { return m_missing; }

public slots:
  // Re-run validation and repaint. Called when the text changes (with a
  // small debounce) and when the parent explicitly asks.
  void revalidateReferences();

  // Remove every line whose referenced path is missing. Returns the
  // number of lines removed.
  int removeMissingReferences();

  signals:
    // Emitted for each dropped file URL that resolves to a local file. The
    // widget does not modify its own contents; the parent decides how to
    // incorporate the paths.
    void filesDropped(const QStringList &paths);

  // Emitted whenever the set of missing references changes.
  void missingReferencesChanged(const QStringList &missing);

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  void applyMissingHighlight();

  QStringList m_missing;
};