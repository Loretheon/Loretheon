#pragma once

#include <QObject>
#include <QString>

class TextEdit;

// Watches a TextEdit for the literal string "@Lore " and, on detection,
// emits triggerDetected with the position of the '@' and the text that
// was typed after it. The caller decides what to do with that.
//
// The trigger text itself is removed from the document before the
// signal fires, so the caller can insert the answer at the position
// the '@' occupied.
class LoreTrigger : public QObject {
  Q_OBJECT

public:
  explicit LoreTrigger(TextEdit *editor, QObject *parent = nullptr);
  ~LoreTrigger() override;

  signals:
    // cursorPosition is the position the '@' occupied in the document
    // before removal. The caller inserts at that position.
    void triggerDetected(int cursorPosition);

private slots:
  void onContentsChanged();

private:
  TextEdit *m_editor = nullptr;
  bool m_processing = false;
};

