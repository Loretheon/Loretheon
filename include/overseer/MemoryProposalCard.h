#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

// A single pending action rendered as an interactive card. Used inside
// the "User actions" tab and mirrored in the transcript for memory
// proposals.
//
// The card does not touch memory.md itself, and it does not touch the
// edit pipeline. It emits accepted(key, scope) / rejected(key) /
// openRequested(key) and lets OverseerWidget route the state change and
// persistence.
//
// Three modes:
//
//   NewFact   — the card proposes a new fact. Scope combo is shown.
//   Replace   — the card proposes replacing an existing fact. Scope
//               combo is shown, and a "replaces: <old>" line is shown
//               above the rationale.
//   Delete    — the card proposes deleting an existing fact. The fact
//               line shows the old text prefixed with a deletion hint.
//               No scope combo; the scope is fixed by the proposal.
//
// An optional "Open" affordance is hidden by default and shown for
// edit-plan actions, where the user needs a way to jump to the plan.
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

  // Configure the card for one of the three modes. `replacedFact` is
  // only meaningful for Replace and Delete.
  void setMode(Mode mode, const QString &replacedFact);

  // Show or hide the scope combo. Hidden by default. When shown, the
  // card seeds it with the given current scope ("global" or "session").
  void setScopeSelectorVisible(bool visible, const QString &currentScope);

  // Show or hide the "Open" button. Hidden by default.
  void setOpenAffordanceVisible(bool visible);
  void setOpenAffordanceLabel(const QString &label);
  void setOpenAffordanceTooltip(const QString &tooltip);

  // Disable every interactive control. Used after an action is applied
  // so the card cannot be clicked twice.
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
  QComboBox *m_scopeCombo = nullptr;
  QPushButton *m_acceptButton = nullptr;
  QPushButton *m_rejectButton = nullptr;
  QPushButton *m_openButton = nullptr;
};