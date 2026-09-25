#pragma once

#include "CardWidget.h"
#include "TranscriptEvent.h"

class MarkdownView;
class TranscriptEditPlanCard;

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;

class TranscriptEventCard : public CardWidget {
  Q_OBJECT

public:
  explicit TranscriptEventCard(const TranscriptEvent &event,
                               QWidget *parent = nullptr);

  void updateEvent(const TranscriptEvent &event);

  const TranscriptEvent &event() const { return m_event; }

signals:
  void memoryProposalAccepted(const QString &key,
                              const QString &scope);
  void memoryProposalRejected(const QString &key);
  void stageRequested(const QString &filePath);

  void planEditAccepted(const QString &planId, int editId);
  void planEditRejected(const QString &planId, int editId);
  void planApplyRequested(const QString &planId);
  void planCancelRequested(const QString &planId);

protected:
  void populateBody(QVBoxLayout *bodyLayout) override;
  QMenu *buildContextMenu(QWidget *parent) override;
  void changeEvent(QEvent *event) override;

private:
  void buildUserAssistant(TranscriptEvent::Type type, QVBoxLayout *bodyLayout);
  void buildToolCall(QVBoxLayout *bodyLayout);
  void buildToolResult(QVBoxLayout *bodyLayout);
  void buildProposal(QVBoxLayout *bodyLayout);
  void buildEditPlan(QVBoxLayout *bodyLayout);
  void buildStage(QVBoxLayout *bodyLayout);
  void buildError(QVBoxLayout *bodyLayout);
  void buildNotice(QVBoxLayout *bodyLayout);

  void refreshProposalControls();

  QString headerTitleFor(const TranscriptEvent &event) const;
  void refreshStatusDot();

  TranscriptEvent m_event;

  MarkdownView *m_bodyView = nullptr;
  QPlainTextEdit *m_bodyEdit = nullptr;

  QWidget *m_proposalButtonRow = nullptr;
  QPushButton *m_proposalAccept = nullptr;
  QPushButton *m_proposalReject = nullptr;
  QComboBox *m_proposalScopeCombo = nullptr;
  QLabel *m_proposalStatusLabel = nullptr;

  TranscriptEditPlanCard *m_planCard = nullptr;
};