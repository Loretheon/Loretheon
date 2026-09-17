#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

// A single pending memory proposal rendered as an interactive card. Used
// inside the "User actions needed" tab. Shows the proposed fact, the
// rationale in smaller text, and Accept / Reject buttons.
//
// The card does not touch memory.md itself. It emits accepted(key) or
// rejected(key) and lets OverseerWidget handle the state change and
// persistence.
class MemoryProposalCard : public QWidget {
  Q_OBJECT

public:
  explicit MemoryProposalCard(const QString &key, const QString &fact,
                              const QString &rationale,
                              QWidget *parent = nullptr);

  QString key() const { return m_key; }

  signals:
    void accepted(const QString &key);
  void rejected(const QString &key);

private:
  QString m_key;

  QLabel *m_factLabel = nullptr;
  QLabel *m_rationaleLabel = nullptr;
  QPushButton *m_acceptButton = nullptr;
  QPushButton *m_rejectButton = nullptr;
};