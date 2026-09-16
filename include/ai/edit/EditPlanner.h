#ifndef EDITPLANNER_H
#define EDITPLANNER_H

#include "PayloadLogger.h"
#include "edit/EditCommand.h"

#include "inference/InferenceService.h"

#include <QObject>
#include <QString>
#include <QVector>

class TextEdit;

class EditPlanner : public QObject {
  Q_OBJECT

public:
  enum class ScopeMode {
    Scoped,
    WholeFile,
  };

  Q_ENUM(ScopeMode)

  explicit EditPlanner(InferenceService *inferenceService,
                       QObject *parent = nullptr);

  void start(TextEdit *editor, const QString &userRequest);

  void start(TextEdit *editor, const QString &userRequest, ScopeMode mode);

  void abort();

  bool isActive() const { return m_active; }

  signals:
    void planValidated(const QVector<EditCommand> &commands);

  void contextScopes(const QStringList &scopeIds);

  void failed(const QString &reason);

private:
  void processStream();

  bool takeCompleteJsonValue(QString &buffer, QString &jsonText);

  InferenceService *m_inferenceService{nullptr};

  TextEdit *m_editor{nullptr};

  QString m_userRequest;

  QString m_streamingResponse;

  PayloadLogger m_payloadLogger;

  ScopeMode m_scopeMode{ScopeMode::Scoped};

  bool m_active{false};

  InferenceService::RequestToken m_activeToken;
};

#endif // EDITPLANNER_H