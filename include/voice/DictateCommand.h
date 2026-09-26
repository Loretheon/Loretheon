#ifndef EPISTEME_DICTATECOMMAND_H
#define EPISTEME_DICTATECOMMAND_H

#include "VoiceCommand.h"

#include <QObject>

class SpeechController;

// Milestone-one command: push to talk, release, and the transcribed
// text is appended to the current document at the cursor.
//
// The command is stateful: while running, it is capturing. Calling
// stop() ends capture and begins transcription.
class DictateCommand : public QObject, public VoiceCommand {
  Q_OBJECT

public:
  explicit DictateCommand(SpeechController *controller,
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
  TextEdit *m_target = nullptr;
};

#endif // EPISTEME_DICTATECOMMAND_H