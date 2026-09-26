#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

// A single pending action rendered as an interactive card. Used inside
// the "User actions" tab and mirrored in the transcript for memory
// proposals.
class MemoryProposalCard : public QWidget {
  Q_OBJECT

public:
  enum class Mode {
    NewFact,
    Replace,
    Delete,
  };

  explicit MemoryProposalCard(const QString &key, const QString &fact,
                              const QString &rationale,
                              QWidget *parent = nullptr);

  QString key() const { return m_key; }

  void setMode(Mode mode, const QString &replacedFact);

  void setScopeSelectorVisible(bool visible, const QString &currentScope);

  void setOpenAffordanceVisible(bool visible);
  void setOpenAffordanceLabel(const QString &label);
  void setOpenAffordanceTooltip(const QString &tooltip);

  // Show a system note under the rationale. Used when a replace fell
  // back to an insert, or when an operation was refused.
  void setNote(const QString &note);

  void setInteractive(bool interactive);

  signals:
    void accepted(const QString &key, const QString &scope);
  void rejected(const QString &key);
  void openRequested(const QString &key);

private:
  QString currentScope() const;

  QString m_key;
  Mode m_mode = Mode::NewFact;

  QLabel *m_factLabel = nullptr;
  QLabel *m_replacedLabel = nullptr;
  QLabel *m_rationaleLabel = nullptr;
  QLabel *m_noteLabel = nullptr;
  QComboBox *m_scopeCombo = nullptr;
  QPushButton *m_acceptButton = nullptr;
  QPushButton *m_rejectButton = nullptr;
  QPushButton *m_openButton = nullptr;
};