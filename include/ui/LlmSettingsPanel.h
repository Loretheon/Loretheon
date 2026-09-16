#ifndef EPISTEME_LLMSETTINGSPANEL_H
#define EPISTEME_LLMSETTINGSPANEL_H

#include <QDialog>
#include <QString>

#include "inference/InferenceService.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class LlmSettingsPanel : public QDialog {
  Q_OBJECT

public:
  explicit LlmSettingsPanel(InferenceService *inferenceService,
                            QWidget *parent = nullptr);

private slots:
  void onModeChanged();
  void onTestClicked();
  void onAccepted();

  void onTestDelta(const InferenceService::RequestToken &token,
                   const QString &text);
  void onTestFinished(const InferenceService::RequestToken &token);
  void onTestError(const InferenceService::RequestToken &token,
                   const QString &error);

private:
  void loadFromSettings();
  void setTestInProgress(bool inProgress);
  void restoreSavedConfig();
  void sendProbe();

  InferenceService *m_inference = nullptr;

  QComboBox *m_modeCombo = nullptr;
  QLineEdit *m_endpointEdit = nullptr;
  QLineEdit *m_modelEdit = nullptr;
  QLineEdit *m_apiKeyEdit = nullptr;
  QComboBox *m_authCombo = nullptr;

  QLabel *m_testStatus = nullptr;
  QPushButton *m_testButton = nullptr;
  QPushButton *m_okButton = nullptr;
  QPushButton *m_cancelButton = nullptr;

  bool m_testInProgress = false;
  QString m_testAccumulator;
  InferenceService::LlmConfig m_savedConfig;

  // Token of the currently in-flight test probe. Null when idle.
  InferenceService::RequestToken m_testToken;
};

#endif // EPISTEME_LLMSETTINGSPANEL_H