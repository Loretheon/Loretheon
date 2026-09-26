#ifndef EPISTEME_READALOUDCOMMAND_H
#define EPISTEME_READALOUDCOMMAND_H

#include "VoiceCommand.h"

#include <QObject>

class SpeechController;

// Milestone-one command: read the current document's text aloud.
class ReadAloudCommand : public QObject, public VoiceCommand {
  Q_OBJECT

public:
  explicit ReadAloudCommand(SpeechController *controller,
                            QObject *parent = nullptr);

  QString id() const override;
  QString title() const override;
  QString description() const override;

  bool canRun(const VoiceContext &context) const override;
  bool isRunning() const override;

  void start(const VoiceContext &context) override;
  void stop() override;

private:
  SpeechController *m_controller = nullptr;
};

#endif // EPISTEME_READALOUDCOMMAND_H