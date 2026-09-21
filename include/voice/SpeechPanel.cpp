#include "../../include/voice/SpeechPanel.h"

#include "../../include/voice/SpeechController.h"
#include "../../include/voice/VoiceCommandRegistry.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

SpeechPanel::SpeechPanel(VoiceCommandRegistry *registry,
                         SpeechController *controller, QWidget *parent)
    : QWidget(parent, Qt::Tool), m_registry(registry),
      m_controller(controller) {
  setWindowTitle(tr("Voice"));
  setMinimumWidth(360);

  m_list = new QListWidget(this);
  m_list->setSelectionMode(QAbstractItemView::SingleSelection);

  m_description = new QLabel(this);
  m_description->setWordWrap(true);
  m_description->setMinimumHeight(48);

  m_runButton = new QPushButton(tr("Run"), this);
  m_stopButton = new QPushButton(tr("Stop"), this);
  m_stopButton->setEnabled(false);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  buttons->addWidget(m_runButton);
  buttons->addWidget(m_stopButton);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(m_list, 1);
  layout->addWidget(m_description);
  layout->addLayout(buttons);

  connect(m_list, &QListWidget::itemSelectionChanged, this,
          &SpeechPanel::onItemSelectionChanged);
  connect(m_runButton, &QPushButton::clicked, this,
          &SpeechPanel::onRunClicked);
  connect(m_stopButton, &QPushButton::clicked, this,
          &SpeechPanel::onStopClicked);

  if (m_registry) {
    connect(m_registry, &VoiceCommandRegistry::changed, this,
            &SpeechPanel::refresh);
  }

  m_stateTimer = new QTimer(this);
  m_stateTimer->setInterval(250);
  connect(m_stateTimer, &QTimer::timeout, this, &SpeechPanel::refresh);
  m_stateTimer->start();

  refresh();
}

void SpeechPanel::setContext(const VoiceContext &context) {
  m_context = context;
  refresh();
}

void SpeechPanel::closeEvent(QCloseEvent *event) {
  event->ignore();
  hide();
}

void SpeechPanel::refresh() {
  if (!m_registry) {
    return;
  }

  const QString previouslySelected =
      m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString()
                            : QString();

  m_list->clear();

  const QVector<VoiceCommand *> commands = m_registry->all();

  for (VoiceCommand *command : commands) {
    if (!command) {
      continue;
    }

    const QString status = statusForCommand(command);
    const QString label = status.isEmpty()
                              ? command->title()
                              : QStringLiteral("%1 — %2")
                                    .arg(command->title(), status);

    auto *item = new QListWidgetItem(label, m_list);
    item->setData(Qt::UserRole, command->id());

    if (!command->canRun(m_context)) {
      item->setForeground(palette().color(QPalette::Disabled,
                                          QPalette::WindowText));
    }

    if (command->id() == previouslySelected) {
      m_list->setCurrentItem(item);
    }
  }

  if (!m_list->currentItem() && m_list->count() > 0) {
    m_list->setCurrentRow(0);
  }

  onItemSelectionChanged();
}

QString SpeechPanel::statusForCommand(VoiceCommand *command) const {
  if (!command || !command->isRunning()) {
    return {};
  }
  return tr("running");
}

VoiceCommand *SpeechPanel::selectedCommand() const {
  if (!m_registry || !m_list->currentItem()) {
    return nullptr;
  }

  const QString id = m_list->currentItem()->data(Qt::UserRole).toString();
  return m_registry->byId(id);
}

void SpeechPanel::onItemSelectionChanged() {
  VoiceCommand *command = selectedCommand();

  if (!command) {
    m_description->clear();
    m_runButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    return;
  }

  m_description->setText(command->description());

  const bool running = command->isRunning();
  m_runButton->setEnabled(!running && command->canRun(m_context));
  m_stopButton->setEnabled(running);
}

void SpeechPanel::onRunClicked() {
  VoiceCommand *command = selectedCommand();
  if (!command || !command->canRun(m_context)) {
    return;
  }

  command->start(m_context);

  if (m_registry) {
    m_registry->notifyChanged();
  }
  refresh();
}

void SpeechPanel::onStopClicked() {
  VoiceCommand *command = selectedCommand();
  if (!command) {
    return;
  }

  command->stop();

  if (m_registry) {
    m_registry->notifyChanged();
  }
  refresh();
}