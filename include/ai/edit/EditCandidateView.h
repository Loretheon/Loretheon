#pragma once

#include "EditMatch.h"

#include <QObject>
#include <QVector>

class QEvent;
class QMouseEvent;
class QTextEdit;
class TextEdit;

class EditCandidateView : public QObject {
  Q_OBJECT

public:
  explicit EditCandidateView(TextEdit *editor = nullptr,
                             QObject *parent = nullptr);

  void setEditor(TextEdit *editor);

  void showCandidates(const QVector<EditMatch> &candidates);

  void clear();

signals:
  void candidateSelected(int index);
  void selectionAborted();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  int candidateAtPosition(const QPoint &position) const;

  TextEdit *m_editor{nullptr};
  QVector<EditMatch> m_candidates;
};