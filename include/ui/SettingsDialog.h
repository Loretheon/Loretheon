#pragma once

#include <QDialog>

class QCheckBox;
class QSpinBox;

class SettingsDialog : public QDialog {
  Q_OBJECT

public:
  explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
  void applyAndClose();

private:
  QSpinBox *m_concurrencyCap = nullptr;
  QCheckBox *m_toasts = nullptr;
  QCheckBox *m_osNotifications = nullptr;
  QCheckBox *m_sound = nullptr;
  QSpinBox *m_fileAgentCap = nullptr;
};