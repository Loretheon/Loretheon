#include "LlmSettingsPanel.h"

#include "Settings.h"
#include "inference/InferenceService.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

LlmSettingsPanel::LlmSettingsPanel(InferenceService *inferenceService,
                                   QWidget *parent)
    : QDialog(parent), m_inference(inferenceService) {
  setWindowTitle(tr("LLM Settings"));
  resize(560, 340);

  auto *root = new QVBoxLayout(this);

  auto *form = new QFormLayout;

  m_modeCombo = new QComboBox(this);
  m_modeCombo->addItem(tr("Local"), QStringLiteral("local"));
  m_modeCombo->addItem(tr("Remote"), QStringLiteral("remote"));

  m_endpointEdit = new QLineEdit(this);
  m_endpointEdit->setPlaceholderText(
      tr("https://api.example.com/v1/chat/completions"));

  m_modelEdit = new QLineEdit(this);
  m_modelEdit->setPlaceholderText(tr("model identifier"));

  m_apiKeyEdit = new QLineEdit(this);
  m_apiKeyEdit->setEchoMode(QLineEdit::Password);
  m_apiKeyEdit->setPlaceholderText(tr("API key"));

  m_authCombo = new QComboBox(this);
  m_authCombo->addItem(tr("Bearer"), QStringLiteral("bearer"));
  m_authCombo->addItem(tr("None"), QStringLiteral("none"));

  form->addRow(tr("Mode:"), m_modeCombo);
  form->addRow(tr("Endpoint:"), m_endpointEdit);
  form->addRow(tr("Model:"), m_modelEdit);
  form->addRow(tr("API key:"), m_apiKeyEdit);
  form->addRow(tr("Auth:"), m_authCombo);

  root->addLayout(form);

  auto *testRow = new QHBoxLayout;
  m_testButton = new QPushButton(tr("Test connection"), this);
  m_testStatus = new QLabel(this);
  m_testStatus->setWordWrap(true);
  testRow->addWidget(m_testButton);
  testRow->addWidget(m_testStatus, 1);
  root->addLayout(testRow);

  root->addStretch();

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  m_okButton = buttons->button(QDialogButtonBox::Ok);
  m_cancelButton = buttons->button(QDialogButtonBox::Cancel);
  root->addWidget(buttons);

  connect(m_modeCombo, &QComboBox::currentIndexChanged, this,
          &LlmSettingsPanel::onModeChanged);
  connect(m_testButton, &QPushButton::clicked, this,
          &LlmSettingsPanel::onTestClicked);
  connect(buttons, &QDialogButtonBox::accepted, this,
          &LlmSettingsPanel::onAccepted);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  if (m_inference) {
    connect(m_inference, &InferenceService::llmDelta, this,
            &LlmSettingsPanel::onTestDelta);
    connect(m_inference, &InferenceService::llmFinished, this,
            &LlmSettingsPanel::onTestFinished);
    connect(m_inference, &InferenceService::llmError, this,
            &LlmSettingsPanel::onTestError);
  }

  loadFromSettings();
  onModeChanged();
  setTestInProgress(false);
}

void LlmSettingsPanel::loadFromSettings() {
  const Settings::LlmSettings settings = Settings::getLlmSettings();

  const int modeIndex = m_modeCombo->findData(settings.mode);
  if (modeIndex >= 0) {
    m_modeCombo->setCurrentIndex(modeIndex);
  } else {
    m_modeCombo->setCurrentIndex(0);
  }

  m_endpointEdit->setText(settings.endpoint);
  m_modelEdit->setText(settings.model);
  m_apiKeyEdit->setText(settings.apiKey);

  const int authIndex = m_authCombo->findData(settings.authType);
  if (authIndex >= 0) {
    m_authCombo->setCurrentIndex(authIndex);
  } else {
    m_authCombo->setCurrentIndex(0);
  }
}

void LlmSettingsPanel::onModeChanged() {
  const bool remote =
      m_modeCombo->currentData().toString() == QStringLiteral("remote");

  m_endpointEdit->setEnabled(remote);
  m_modelEdit->setEnabled(remote);
  m_apiKeyEdit->setEnabled(remote);
  m_authCombo->setEnabled(remote);
  m_testButton->setEnabled(remote && !m_testInProgress);
}

void LlmSettingsPanel::setTestInProgress(bool inProgress) {
  m_testInProgress = inProgress;

  const bool remote =
      m_modeCombo->currentData().toString() == QStringLiteral("remote");

  m_testButton->setEnabled(remote && !inProgress);
  m_okButton->setEnabled(!inProgress);
  m_cancelButton->setEnabled(!inProgress);
}

void LlmSettingsPanel::onTestClicked() {
  if (!m_inference) {
    m_testStatus->setText(tr("Inference service unavailable."));
    return;
  }

  if (m_testInProgress) {
    return;
  }

  const QString endpoint = m_endpointEdit->text().trimmed();
  const QString model = m_modelEdit->text().trimmed();

  if (endpoint.isEmpty() || model.isEmpty()) {
    m_testStatus->setText(tr("Endpoint and model are required."));
    return;
  }

  m_savedConfig = InferenceService::LlmConfig();
  m_savedConfig.mode = m_inference->llmMode();
  m_savedConfig.endpoint = m_inference->llmEndpoint();
  m_savedConfig.model = m_inference->llmModel();

  const Settings::LlmSettings stored = Settings::getLlmSettings();
  m_savedConfig.apiKey = stored.apiKey;
  m_savedConfig.authType =
      stored.authType == QStringLiteral("none")
          ? InferenceService::LlmAuthType::None
          : InferenceService::LlmAuthType::Bearer;

  InferenceService::LlmConfig probe;
  probe.mode = InferenceService::LlmMode::Remote;
  probe.endpoint = endpoint;
  probe.model = model;
  probe.apiKey = m_apiKeyEdit->text().trimmed();
  probe.authType =
      m_authCombo->currentData().toString() == QStringLiteral("none")
          ? InferenceService::LlmAuthType::None
          : InferenceService::LlmAuthType::Bearer;

  m_testAccumulator.clear();
  m_testStatus->setText(tr("Testing…"));
  setTestInProgress(true);

  if (!m_inference->setLlmConfig(probe)) {
    m_testStatus->setText(tr("Could not apply the test configuration."));
    setTestInProgress(false);
    restoreSavedConfig();
    return;
  }

  if (m_inference->isLlmReady()) {
    sendProbe();
  } else {
    connect(
        m_inference, &InferenceService::llmReady, this,
        [this]() { sendProbe(); },
        Qt::SingleShotConnection);
  }
}

void LlmSettingsPanel::sendProbe() {
  if (!m_inference || !m_testInProgress) {
    return;
  }

  QJsonObject userMessage;
  userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
  userMessage.insert(QStringLiteral("content"),
                     QStringLiteral("Reply with the single word: ok"));

  QJsonArray messages;
  messages.append(userMessage);

  m_testToken = m_inference->sendChatRequest(
      messages, QString(), 0.0, 20000);
}

void LlmSettingsPanel::onTestDelta(const InferenceService::RequestToken &token,
                                   const QString &text) {
  if (token != m_testToken || !m_testInProgress) {
    return;
  }

  m_testAccumulator += text;
}

void LlmSettingsPanel::onTestFinished(
    const InferenceService::RequestToken &token) {
  if (token != m_testToken || !m_testInProgress) {
    return;
  }

  m_testToken = InferenceService::RequestToken();
  setTestInProgress(false);

  const QString reply = m_testAccumulator.trimmed();

  if (reply.isEmpty()) {
    m_testStatus->setText(tr("Reached the endpoint, but the reply was empty."));
  } else {
    m_testStatus->setText(tr("OK — reply: %1").arg(reply.left(160)));
  }

  restoreSavedConfig();
}

void LlmSettingsPanel::onTestError(const InferenceService::RequestToken &token,
                                   const QString &error) {
  if (token != m_testToken || !m_testInProgress) {
    return;
  }

  m_testToken = InferenceService::RequestToken();
  setTestInProgress(false);
  m_testStatus->setText(tr("Failed: %1").arg(error));

  restoreSavedConfig();
}

void LlmSettingsPanel::restoreSavedConfig() {
  if (!m_inference) {
    return;
  }

  InferenceService::LlmConfig current;
  current.mode = m_savedConfig.mode;
  current.endpoint = m_savedConfig.endpoint;
  current.model = m_savedConfig.model;
  current.apiKey = m_savedConfig.apiKey;
  current.authType = m_savedConfig.authType;

  if (current.apiKey.isEmpty()) {
    const Settings::LlmSettings stored = Settings::getLlmSettings();
    current.apiKey = stored.apiKey;
  }

  m_inference->setLlmConfig(current);
}

void LlmSettingsPanel::onAccepted() {
  Settings::LlmSettings settings;
  settings.mode = m_modeCombo->currentData().toString();
  settings.endpoint = m_endpointEdit->text().trimmed();
  settings.model = m_modelEdit->text().trimmed();
  settings.apiKey = m_apiKeyEdit->text().trimmed();
  settings.authType = m_authCombo->currentData().toString();

  Settings::setLlmSettings(settings);

  if (m_inference) {
    InferenceService::LlmConfig config;

    if (settings.mode == QStringLiteral("remote")) {
      config.mode = InferenceService::LlmMode::Remote;
      config.endpoint = settings.endpoint;
      config.model = settings.model;
      config.apiKey = settings.apiKey;
      config.authType =
          settings.authType == QStringLiteral("none")
              ? InferenceService::LlmAuthType::None
              : InferenceService::LlmAuthType::Bearer;
    } else {
      config.mode = InferenceService::LlmMode::Local;
    }

    m_inference->setLlmConfig(config);
  }

  accept();
}