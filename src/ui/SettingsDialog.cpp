#include "../../include/ui/SettingsDialog.h"

#include "NotificationService.h"
#include "Settings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Settings"));
  setMinimumWidth(420);

  auto *layout = new QVBoxLayout(this);

  auto *overseerLabel = new QLabel(tr("Overseer"), this);
  QFont bold = overseerLabel->font();
  bold.setBold(true);
  overseerLabel->setFont(bold);
  layout->addWidget(overseerLabel);

  auto *overseerForm = new QFormLayout;

  m_concurrencyCap = new QSpinBox(this);
  m_concurrencyCap->setRange(1, 16);
  m_concurrencyCap->setValue(Settings::getOverseerConcurrencyCap());
  m_concurrencyCap->setToolTip(
      tr("How many files can be under concurrent scoped edit at once."));

  overseerForm->addRow(tr("Concurrent edits:"), m_concurrencyCap);


  m_fileAgentCap = new QSpinBox(this);
  m_fileAgentCap->setRange(1, 16);
  m_fileAgentCap->setValue(Settings::getOverseerFileAgentCap());
  m_fileAgentCap->setToolTip(
      tr("How many file-manipulation agents can run in parallel."));

  overseerForm->addRow(tr("File agents:"), m_fileAgentCap);
  

  layout->addLayout(overseerForm);

  layout->addSpacing(12);

  auto *notifLabel = new QLabel(tr("Notifications"), this);
  notifLabel->setFont(bold);
  layout->addWidget(notifLabel);

  auto *notifForm = new QFormLayout;

  auto &service = NotificationService::instance();

  m_toasts = new QCheckBox(tr("Show toasts"), this);
  m_toasts->setChecked(service.toastsEnabled());
  notifForm->addRow(QString(), m_toasts);

  m_osNotifications =
      new QCheckBox(tr("Show OS notifications for critical events"), this);
  m_osNotifications->setChecked(service.osNotificationsEnabled());
  notifForm->addRow(QString(), m_osNotifications);

  m_sound = new QCheckBox(tr("Play a sound"), this);
  m_sound->setChecked(service.soundEnabled());
  notifForm->addRow(QString(), m_sound);

  layout->addLayout(notifForm);
  layout->addStretch(1);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                       QDialogButtonBox::Cancel, this);

  connect(buttons, &QDialogButtonBox::accepted, this,
          &SettingsDialog::applyAndClose);

  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  layout->addWidget(buttons);
}

void SettingsDialog::applyAndClose() {
  Settings::setOverseerConcurrencyCap(m_concurrencyCap->value());

  auto &service = NotificationService::instance();

  service.setToastsEnabled(m_toasts->isChecked());
  service.setOsNotificationsEnabled(m_osNotifications->isChecked());
  service.setSoundEnabled(m_sound->isChecked());

  accept();
}