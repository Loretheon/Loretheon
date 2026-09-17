#include "../../include/overseer/AutomationStrip.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

AutomationStrip::AutomationStrip(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("overseerAutomationStrip"));

  auto *row = new QHBoxLayout(this);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(10);

  m_automaticCheck = new QCheckBox(tr("Automatic"), this);
  m_automaticCheck->setToolTip(
      tr("Approve memory proposals and apply edit plans without asking."));

  m_memoryCheck = new QCheckBox(tr("Auto memory"), this);
  m_memoryCheck->setToolTip(tr("Accept every memory proposal automatically."));

  m_editsCheck = new QCheckBox(tr("Auto edits"), this);
  m_editsCheck->setToolTip(tr("Apply every edit plan automatically."));

  row->addWidget(m_automaticCheck);
  row->addWidget(m_memoryCheck);
  row->addWidget(m_editsCheck);

  m_automaticIndicator = new QWidget(this);
  auto *indicatorRow = new QHBoxLayout(m_automaticIndicator);
  indicatorRow->setContentsMargins(0, 0, 0, 0);
  indicatorRow->setSpacing(8);

  m_automaticLabel = new QLabel(tr("Automatic mode"), m_automaticIndicator);
  {
    QFont bold = m_automaticLabel->font();
    bold.setBold(true);
    m_automaticLabel->setFont(bold);
  }

  m_offButton = new QPushButton(tr("Off"), m_automaticIndicator);
  m_offButton->setAutoDefault(false);

  indicatorRow->addWidget(m_automaticLabel);
  indicatorRow->addWidget(m_offButton);

  row->addWidget(m_automaticIndicator);
  row->addStretch(1);

  connect(m_automaticCheck, &QCheckBox::toggled, this,
          &AutomationStrip::onAutomaticToggled);
  connect(m_memoryCheck, &QCheckBox::toggled, this,
          &AutomationStrip::onMemoryToggled);
  connect(m_editsCheck, &QCheckBox::toggled, this,
          &AutomationStrip::onEditsToggled);
  connect(m_offButton, &QPushButton::clicked, this,
          &AutomationStrip::onOffClicked);

  applyToUi();
}

void AutomationStrip::setSettings(const SessionSettings &settings) {
  m_settings = settings;
  m_settings.normalize();
  applyToUi();
}

void AutomationStrip::setEnabledState(bool enabled) {
  setEnabled(enabled);

  if (m_automaticCheck)
    m_automaticCheck->setEnabled(enabled);
  if (m_memoryCheck)
    m_memoryCheck->setEnabled(enabled);
  if (m_editsCheck)
    m_editsCheck->setEnabled(enabled);
  if (m_offButton)
    m_offButton->setEnabled(enabled);
}

void AutomationStrip::onAutomaticToggled(bool on) {
  if (m_updating)
    return;

  m_settings.automatic = on;
  m_settings.normalize();

  applyToUi();
  emitChanged();
}

void AutomationStrip::onMemoryToggled(bool on) {
  if (m_updating)
    return;

  m_settings.autoMemory = on;

  if (!on)
    m_settings.automatic = false;

  m_settings.normalize();

  applyToUi();
  emitChanged();
}

void AutomationStrip::onEditsToggled(bool on) {
  if (m_updating)
    return;

  m_settings.autoEdits = on;

  if (!on)
    m_settings.automatic = false;

  m_settings.normalize();

  applyToUi();
  emitChanged();
}

void AutomationStrip::onOffClicked() {
  m_settings.automatic = false;
  m_settings.autoMemory = false;
  m_settings.autoEdits = false;

  applyToUi();
  emitChanged();
}

void AutomationStrip::applyToUi() {
  m_updating = true;

  {
    QSignalBlocker b1(m_automaticCheck);
    QSignalBlocker b2(m_memoryCheck);
    QSignalBlocker b3(m_editsCheck);

    m_automaticCheck->setChecked(m_settings.automatic);
    m_memoryCheck->setChecked(m_settings.effectiveAutoMemory());
    m_editsCheck->setChecked(m_settings.effectiveAutoEdits());
  }

  const bool automatic = m_settings.automatic;

  m_automaticCheck->setVisible(!automatic);
  m_memoryCheck->setVisible(!automatic);
  m_editsCheck->setVisible(!automatic);

  m_automaticIndicator->setVisible(automatic);

  m_updating = false;
}

void AutomationStrip::emitChanged() {
  SessionSettings out = m_settings;
  out.normalize();
  emit settingsChanged(out);
}