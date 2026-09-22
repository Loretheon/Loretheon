#ifndef EPISTEME_LIVEDICTATECOMMAND_H
#define EPISTEME_LIVEDICTATECOMMAND_H

#include "VoiceCommand.h"

#include <QObject>

class SpeechController;

// Live dictation: speak continuously, text appears in place as the
// model refines. Interim results replace the previous interim; final
// results replace the last interim and stay.
//
// The anchor is a start position and a length, not a QTextCursor. That
// keeps it cheap to reason about and immune to Qt's cursor state.
class LiveDictateCommand : public QObject, public VoiceCommand {
  Q_OBJECT

public:
  explicit LiveDictateCommand(SpeechController *controller,
                              QObject *parent = nullptr);

  QString id() const override;
  QString title() const override;
  QString description() const override;

  bool canRun(const VoiceContext &context) const override;
  bool isRunning() const override;

  void start(const VoiceContext &context) override;
  void stop() override;

private slots:
  void onLiveTranscribed(const QString &text, bool isFinal);

private:
  void clearAnchor();

  SpeechController *m_controller = nullptr;
  TextEdit *m_target = nullptr;

  // Position in the document where the current utterance began. -1
  // means no utterance is pending. m_anchorLength is how many
  // characters of interim text are currently occupying that range.
  int m_anchorStart = -1;
  int m_anchorLength = 0;
};

#endif // EPISTEME_LIVEDICTATECOMMAND_H