#pragma once

#include "CardWidget.h"
#include "TranscriptEvent.h"

class MarkdownView;

class QLabel;
class QPlainTextEdit;
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
    void memoryProposalAccepted(const QString &key);
  void memoryProposalRejected(const QString &key);
  void stageRequested(const QString &filePath);

protected:
  void populateBody(QVBoxLayout *bodyLayout) override;
  QMenu *buildContextMenu(QWidget *parent) override;
  void changeEvent(QEvent *event) override;

private:
  void buildUserAssistant(TranscriptEvent::Type type, QVBoxLayout *bodyLayout);
  void buildToolCall(QVBoxLayout *bodyLayout);
  void buildToolResult(QVBoxLayout *bodyLayout);
  void buildProposal(QVBoxLayout *bodyLayout);
  void buildStage(QVBoxLayout *bodyLayout);
  void buildError(QVBoxLayout *bodyLayout);
  void buildNotice(QVBoxLayout *bodyLayout);

  QString headerTitleFor(const TranscriptEvent &event) const;
  void refreshStatusDot();

  TranscriptEvent m_event;

  MarkdownView *m_bodyView = nullptr;
  QPlainTextEdit *m_bodyEdit = nullptr;
  QLabel *m_metaLabel = nullptr;
};