#include "../../include/overseer/TranscriptPanel.h"

#include "../../include/overseer/TranscriptEventCard.h"
#include "../../include/overseer/TranscriptRibbon.h"
#include "../../include/overseer/TranscriptStore.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr auto PrefUserKey = "overseer/transcript/showUser";
constexpr auto PrefAssistantKey = "overseer/transcript/showAssistant";
constexpr auto PrefToolsKey = "overseer/transcript/showTools";
constexpr auto PrefProposalsKey = "overseer/transcript/showProposals";
constexpr auto PrefErrorsKey = "overseer/transcript/showErrors";
constexpr auto PrefTimestampsKey = "overseer/transcript/showTimestamps";
constexpr auto PrefPinToBottomKey = "overseer/transcript/pinToBottom";

constexpr int kBottomTolerancePx = 4;

} // namespace

TranscriptPanel::TranscriptPanel(TranscriptStore *store, QWidget *parent)
    : QWidget(parent), m_store(store) {
  setObjectName(QStringLiteral("transcriptPanel"));

  m_ribbon = new TranscriptRibbon(this);

  auto *filterRow = new QHBoxLayout;
  filterRow->setContentsMargins(4, 2, 4, 2);
  filterRow->setSpacing(8);

  m_showUser = new QCheckBox(tr("Me"), this);
  m_showAssistant = new QCheckBox(tr("Overseer"), this);
  m_showTools = new QCheckBox(tr("Tools"), this);
  m_showProposals = new QCheckBox(tr("Proposals"), this);
  m_showErrors = new QCheckBox(tr("Errors"), this);
  m_showTimestampsCheck = new QCheckBox(tr("Times"), this);
  m_pinToBottomCheck = new QCheckBox(tr("Follow"), this);
  m_pinToBottomCheck->setToolTip(
      tr("Stick the transcript to the newest event as it arrives."));

  filterRow->addWidget(m_showUser);
  filterRow->addWidget(m_showAssistant);
  filterRow->addWidget(m_showTools);
  filterRow->addWidget(m_showProposals);
  filterRow->addWidget(m_showErrors);
  filterRow->addStretch(1);
  filterRow->addWidget(m_pinToBottomCheck);
  filterRow->addWidget(m_showTimestampsCheck);

  m_scroll = new QScrollArea(this);
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);

  m_cardsHost = new QWidget;
  m_cardsLayout = new QVBoxLayout(m_cardsHost);
  m_cardsLayout->setContentsMargins(8, 8, 8, 8);
  m_cardsLayout->setSpacing(8);
  m_cardsLayout->setAlignment(Qt::AlignTop);

  m_emptyLabel = new QLabel(tr("No events yet."), m_cardsHost);
  m_emptyLabel->setAlignment(Qt::AlignCenter);
  m_cardsLayout->addWidget(m_emptyLabel);
  m_cardsLayout->addStretch(1);

  m_scroll->setWidget(m_cardsHost);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);
  root->addWidget(m_ribbon);
  root->addLayout(filterRow);
  root->addWidget(m_scroll, 1);

  connect(m_ribbon, &TranscriptRibbon::jumpRequested, this,
          &TranscriptPanel::onRibbonJumpRequested);

  if (m_store) {
    connect(m_store, &TranscriptStore::eventsReset, this,
            &TranscriptPanel::onEventsReset);
    connect(m_store, &TranscriptStore::eventAppended, this,
            &TranscriptPanel::onEventAppended);
    connect(m_store, &TranscriptStore::eventUpdated, this,
            &TranscriptPanel::onEventUpdated);
  }

  if (m_scroll) {
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) {
              updateRibbonVisibleRange();

              if (m_snapPending && !isScrollbarAtBottom())
                m_snapPending = false;
            });
  }

  auto rebuildFromFilter = [this]() {
    savePreferences();
    rebuildAllCards();
  };

  connect(m_showUser, &QCheckBox::toggled, this, rebuildFromFilter);
  connect(m_showAssistant, &QCheckBox::toggled, this, rebuildFromFilter);
  connect(m_showTools, &QCheckBox::toggled, this, rebuildFromFilter);
  connect(m_showProposals, &QCheckBox::toggled, this, rebuildFromFilter);
  connect(m_showErrors, &QCheckBox::toggled, this, rebuildFromFilter);

  connect(m_showTimestampsCheck, &QCheckBox::toggled, this, [this](bool on) {
    m_showTimestamps = on;
    savePreferences();
    rebuildAllCards();
  });

  connect(m_pinToBottomCheck, &QCheckBox::toggled, this, [this](bool on) {
    m_pinToBottom = on;
    savePreferences();

    if (on)
      snapToBottom();
  });

  loadPreferences();
  onEventsReset();
}

void TranscriptPanel::loadPreferences() {
  QSettings s;

  m_showUser->setChecked(s.value(PrefUserKey, true).toBool());
  m_showAssistant->setChecked(s.value(PrefAssistantKey, true).toBool());
  m_showTools->setChecked(s.value(PrefToolsKey, true).toBool());
  m_showProposals->setChecked(s.value(PrefProposalsKey, true).toBool());
  m_showErrors->setChecked(s.value(PrefErrorsKey, true).toBool());

  m_showTimestamps = s.value(PrefTimestampsKey, true).toBool();
  m_showTimestampsCheck->setChecked(m_showTimestamps);

  m_pinToBottom = s.value(PrefPinToBottomKey, false).toBool();
  m_pinToBottomCheck->setChecked(m_pinToBottom);
}

void TranscriptPanel::savePreferences() {
  QSettings s;

  s.setValue(PrefUserKey, m_showUser->isChecked());
  s.setValue(PrefAssistantKey, m_showAssistant->isChecked());
  s.setValue(PrefToolsKey, m_showTools->isChecked());
  s.setValue(PrefProposalsKey, m_showProposals->isChecked());
  s.setValue(PrefErrorsKey, m_showErrors->isChecked());
  s.setValue(PrefTimestampsKey, m_showTimestamps);
  s.setValue(PrefPinToBottomKey, m_pinToBottom);
}

void TranscriptPanel::setShowTimestamps(bool show) {
  if (m_showTimestamps == show)
    return;

  m_showTimestamps = show;
  m_showTimestampsCheck->setChecked(show);
}

void TranscriptPanel::setPinToBottom(bool pin) {
  if (m_pinToBottom == pin)
    return;

  m_pinToBottom = pin;
  m_pinToBottomCheck->setChecked(pin);
  savePreferences();

  if (pin)
    snapToBottom();
}

bool TranscriptPanel::passesFilter(const TranscriptEvent &event) const {
  switch (event.type) {
  case TranscriptEvent::Type::UserMessage:
    return m_showUser->isChecked();
  case TranscriptEvent::Type::AssistantMessage:
    return m_showAssistant->isChecked();
  case TranscriptEvent::Type::ToolCall:
  case TranscriptEvent::Type::ToolResult:
    return m_showTools->isChecked();
  case TranscriptEvent::Type::MemoryProposal:
    return m_showProposals->isChecked();
  case TranscriptEvent::Type::Error:
    return m_showErrors->isChecked();
  case TranscriptEvent::Type::Stage:
  case TranscriptEvent::Type::Promotion:
  case TranscriptEvent::Type::Notice:
    return true;
  }
  return true;
}

void TranscriptPanel::clearAllCards() {
  const auto existing = m_cardsHost->findChildren<TranscriptEventCard *>(
      QString(), Qt::FindDirectChildrenOnly);

  for (auto *card : existing) {
    m_cardsLayout->removeWidget(card);
    card->deleteLater();
  }

  m_eventCards.clear();
}

void TranscriptPanel::onEventsReset() {
  if (!m_store)
    return;

  m_ribbon->setEvents(m_store->events());

  rebuildAllCards();

  if (m_pinToBottom)
    snapToBottom();

  updateRibbonVisibleRange();
}

void TranscriptPanel::onEventAppended(int index) {
  if (!m_store)
    return;

  m_ribbon->setEvents(m_store->events());

  appendCardForEvent(index);

  if (m_pinToBottom)
    snapToBottom();

  updateRibbonVisibleRange();
}

void TranscriptPanel::onEventUpdated(int index) {
  if (!m_store)
    return;

  if (m_eventCards.contains(index)) {
    auto *card = m_eventCards.value(index);

    if (card)
      card->updateEvent(m_store->events().at(index));
  }
}

void TranscriptPanel::onRibbonJumpRequested(int index) {
  scrollToEventCard(index);
}

void TranscriptPanel::rebuildAllCards() {
  clearAllCards();

  if (!m_store) {
    m_emptyLabel->setVisible(true);
    return;
  }

  const auto &events = m_store->events();

  int visible = 0;

  for (int i = 0; i < events.size(); ++i) {
    if (!passesFilter(events.at(i)))
      continue;

    auto *card = new TranscriptEventCard(events.at(i), m_cardsHost);

    connect(card, &TranscriptEventCard::memoryProposalAccepted, this,
            &TranscriptPanel::memoryProposalAccepted);

    connect(card, &TranscriptEventCard::memoryProposalRejected, this,
            &TranscriptPanel::memoryProposalRejected);

    connect(card, &TranscriptEventCard::planEditAccepted, this,
            &TranscriptPanel::planEditAccepted);

    connect(card, &TranscriptEventCard::planEditRejected, this,
            &TranscriptPanel::planEditRejected);

    connect(card, &TranscriptEventCard::planApplyRequested, this,
            &TranscriptPanel::planApplyRequested);

    connect(card, &TranscriptEventCard::planCancelRequested, this,
            &TranscriptPanel::planCancelRequested);

    m_cardsLayout->insertWidget(visible, card);
    m_eventCards.insert(i, card);

    ++visible;
  }

  m_emptyLabel->setVisible(visible == 0);
}

void TranscriptPanel::appendCardForEvent(int index) {
  if (!m_store)
    return;

  const auto &events = m_store->events();

  if (index < 0 || index >= events.size())
    return;

  if (!passesFilter(events.at(index)))
    return;

  if (m_eventCards.contains(index))
    return;

  auto *card = new TranscriptEventCard(events.at(index), m_cardsHost);

  connect(card, &TranscriptEventCard::memoryProposalAccepted, this,
          &TranscriptPanel::memoryProposalAccepted);

  connect(card, &TranscriptEventCard::memoryProposalRejected, this,
          &TranscriptPanel::memoryProposalRejected);

  connect(card, &TranscriptEventCard::planEditAccepted, this,
          &TranscriptPanel::planEditAccepted);

  connect(card, &TranscriptEventCard::planEditRejected, this,
          &TranscriptPanel::planEditRejected);

  connect(card, &TranscriptEventCard::planApplyRequested, this,
          &TranscriptPanel::planApplyRequested);

  connect(card, &TranscriptEventCard::planCancelRequested, this,
          &TranscriptPanel::planCancelRequested);

  const int insertAt = m_cardsLayout->count() - 2;
  m_cardsLayout->insertWidget(qMax(0, insertAt), card);
  m_eventCards.insert(index, card);

  m_emptyLabel->setVisible(false);
}

bool TranscriptPanel::isScrollbarAtBottom() const {
  if (!m_scroll)
    return true;

  QScrollBar *bar = m_scroll->verticalScrollBar();

  if (!bar)
    return true;

  return bar->value() >= bar->maximum() - kBottomTolerancePx;
}

void TranscriptPanel::snapToBottom() {
  if (!m_scroll)
    return;

  m_snapPending = true;

  QPointer<TranscriptPanel> guard(this);

  QTimer::singleShot(0, this, [this, guard]() {
    if (!guard || !m_scroll)
      return;

    if (!m_pinToBottom) {
      m_snapPending = false;
      return;
    }

    m_cardsLayout->activate();

    QScrollBar *bar = m_scroll->verticalScrollBar();

    if (bar)
      bar->setValue(bar->maximum());

    updateRibbonVisibleRange();

    QTimer::singleShot(0, this, [this, guard]() {
      if (!guard || !m_scroll)
        return;

      if (!m_pinToBottom) {
        m_snapPending = false;
        return;
      }

      QScrollBar *bar = m_scroll->verticalScrollBar();

      if (bar)
        bar->setValue(bar->maximum());

      updateRibbonVisibleRange();

      m_snapPending = false;
    });
  });
}

void TranscriptPanel::scrollToEventCard(int index) {
  if (!m_store)
    return;

  const auto &events = m_store->events();

  if (events.isEmpty())
    return;

  const int clampedIndex = qBound(0, index, events.size() - 1);

  TranscriptEventCard *card = nullptr;

  for (int i = clampedIndex; i < events.size(); ++i) {
    if (m_eventCards.contains(i)) {
      card = m_eventCards.value(i);
      break;
    }
  }

  if (!card) {
    for (int i = clampedIndex - 1; i >= 0; --i) {
      if (m_eventCards.contains(i)) {
        card = m_eventCards.value(i);
        break;
      }
    }
  }

  if (!card || !m_scroll)
    return;

  m_scroll->ensureWidgetVisible(card, 0, 0);

  updateRibbonVisibleRange();
}

void TranscriptPanel::updateRibbonVisibleRange() {
  if (!m_scroll || !m_ribbon || !m_store)
    return;

  const auto &events = m_store->events();

  if (events.isEmpty()) {
    m_ribbon->setVisibleRange(0, 0);
    return;
  }

  const int viewportTop = m_scroll->verticalScrollBar()->value();
  const int viewportBottom = viewportTop + m_scroll->viewport()->height();

  int firstVisible = -1;
  int lastVisible = -1;

  for (auto it = m_eventCards.constBegin(); it != m_eventCards.constEnd();
       ++it) {
    auto *card = it.value();

    if (!card)
      continue;

    const int top = card->mapTo(m_cardsHost, QPoint(0, 0)).y();
    const int bottom = top + card->height();

    const bool intersects = bottom > viewportTop && top < viewportBottom;

    if (!intersects)
      continue;

    if (firstVisible < 0 || it.key() < firstVisible)
      firstVisible = it.key();

    if (it.key() > lastVisible)
      lastVisible = it.key();
  }

  if (firstVisible < 0) {
    m_ribbon->setVisibleRange(0, 0);
    return;
  }

  m_ribbon->setVisibleRange(firstVisible, lastVisible);
}

void TranscriptPanel::setStore(TranscriptStore *store) {
  if (m_store == store)
    return;

  if (m_store) {
    disconnect(m_store, nullptr, this, nullptr);
  }

  m_store = store;

  clearAllCards();

  if (m_store) {
    connect(m_store, &TranscriptStore::eventsReset, this,
            &TranscriptPanel::onEventsReset);
    connect(m_store, &TranscriptStore::eventAppended, this,
            &TranscriptPanel::onEventAppended);
    connect(m_store, &TranscriptStore::eventUpdated, this,
            &TranscriptPanel::onEventUpdated);
  }

  onEventsReset();
}