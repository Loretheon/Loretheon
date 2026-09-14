#pragma once

#include "EditCommand.h"
#include "EditMatch.h"

#include <QObject>

class ChatWidget;

class ChatWidgetEditFlow : public QObject {
  Q_OBJECT

public:
  explicit ChatWidgetEditFlow(ChatWidget *widget);

public slots:

  void sendPrompt(const QString &prompt);

  void requestNextEditCommand();

  void onPlanValidated(const QVector<EditCommand> &commands);

  void onPlanApprovalRequested(const QVector<EditCommand> &editedCommands);

  void beginStreamingResolvedPlan();

  void executeNextPlannedEdit();

  void requestEditContent();

  void onPlanReady(const QVector<EditCommand> &commands);

  void onPlanFailed(const QString &reason);

  void onEditCandidatesReady(const QVector<EditMatch> &candidates);

  void onEditApplied(bool fuzzy, int editDistance);

  void onEditFailed(const QString &reason);

  void onEditAborted();

  void onReviewReady();

  void resetState();

private:
  QString describeCommand(const EditCommand &command) const;

private:
  ChatWidget *m_widget = nullptr;
};