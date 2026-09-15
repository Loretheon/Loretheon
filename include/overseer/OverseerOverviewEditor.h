#pragma once

#include <QPlainTextEdit>

class OverseerOverviewEditor : public QPlainTextEdit {
  Q_OBJECT

public:
  explicit OverseerOverviewEditor(QWidget *parent = nullptr);

  signals:
    // Emitted for each dropped file URL that resolves to a local file. The
    // widget does not modify its own contents; the parent decides how to
    // incorporate the paths.
    void filesDropped(const QStringList &paths);

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
};