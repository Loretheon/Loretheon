#ifndef EDITPLANNER_H
#define EDITPLANNER_H

#include "PayloadLogger.h"
#include "edit/EditCommand.h"

#include <QObject>
#include <QString>
#include <QVector>

class InferenceService;
class TextEdit;

class EditPlanner : public QObject {
  Q_OBJECT

public:
  explicit EditPlanner(InferenceService *inferenceService,
                       QObject *parent = nullptr);

  void start(TextEdit *editor, const QString &userRequest);

  void abort();

signals:
  void planReady(const QVector<EditCommand> &commands);

  void failed(const QString &reason);

private slots:
  void onLlmDelta(const QString &text);

  void onLlmFinished();

  void onLlmError(const QString &error);

private:
  void processStream();

  bool takeCompleteJsonValue(QString &buffer, QString &jsonText);

  InferenceService *m_inferenceService{nullptr};

  TextEdit *m_editor{nullptr};

  QString m_userRequest;

  QString m_streamingResponse;

  PayloadLogger m_payloadLogger;

  bool m_active{false};
};

#endif // EDITPLANNER_H