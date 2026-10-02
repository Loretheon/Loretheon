#pragma once

#include <QHash>
#include <QWidget>

#include "TranscriptEvent.h"

class TranscriptEventCard;
class TranscriptRibbon;
class TranscriptStore;

class QVBoxLayout;
class QHBoxLayout;
class QScrollArea;
class QCheckBox;
class QLabel;

class TranscriptPanel : public QWidget {
  Q_OBJECT

public:
  explicit TranscriptPanel(TranscriptStore *store, QWidget *parent = nullptr);

  void setStore(TranscriptStore *store);
  TranscriptStore *store() const { return m_store; }

  void setShowTimestamps(bool show);
  bool showTimestamps() const { return m_showTimestamps; }

  void setPinToBottom(bool pin);
  bool pinToBottom() const { return m_pinToBottom; }

  // Which edge of the page the panel is docked to. Determines whether
  // the panel lays out vertically (Left/Right) or horizontally
  // (Top/Bottom). The ribbon is hidden in horizontal mode because its
  // vertical minimap metaphor does not translate to a short strip.
  void setDockOrientation(Qt::Orientation orientation);
  Qt::Orientation dockOrientation() const { return m_orientation; }

signals:
  void memoryProposalAccepted(const QString &key, const QString &scope);
  void memoryProposalRejected(const QString &key);

  void planEditAccepted(const QString &planId, int editId);
  void planEditRejected(const QString &planId, int editId);
  void planApplyRequested(const QString &planId);
  void planCancelRequested(const QString &planId);

private slots:
  void onEventsReset();
  void onEventAppended(int index);
  void onEventUpdated(int index);

  void onRibbonJumpRequested(int index);

  void rebuildAllCards();
  void appendCardForEvent(int index);

private:
  void loadPreferences();
  void savePreferences();

  void relayoutForOrientation();

  bool passesFilter(const TranscriptEvent &event) const;

  void snapToBottom();
  void scrollToEventCard(int index);
  void updateRibbonVisibleRange();

  void clearAllCards();

  bool isScrollbarAtBottom() const;

  TranscriptStore *m_store = nullptr;

  Qt::Orientation m_orientation = Qt::Vertical;

  TranscriptRibbon *m_ribbon = nullptr;
  QScrollArea *m_scroll = nullptr;
  QWidget *m_cardsHost = nullptr;
  QVBoxLayout *m_cardsLayout = nullptr;
  QLabel *m_emptyLabel = nullptr;

  QWidget *m_filterRow = nullptr;
  QCheckBox *m_showUser = nullptr;
  QCheckBox *m_showAssistant = nullptr;
  QCheckBox *m_showTools = nullptr;
  QCheckBox *m_showProposals = nullptr;
  QCheckBox *m_showErrors = nullptr;
  QCheckBox *m_showTimestampsCheck = nullptr;
  QCheckBox *m_pinToBottomCheck = nullptr;

  bool m_showTimestamps = true;
  bool m_pinToBottom = false;

  QHash<int, TranscriptEventCard *> m_eventCards;

  bool m_snapPending = false;
};