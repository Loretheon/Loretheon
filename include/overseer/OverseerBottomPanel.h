#pragma once

#include <QWidget>

class QVBoxLayout;
class AutomationStrip;
class TranscriptPanel;

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

// The whole interaction surface of an Overseer session, folded into a
// single widget so the bottom auto-hide dock can carry it as one piece.
//
// Top to bottom:
//
//   [session header]
//   [automation strip]
//   [transcript scroll area]
//   [composer row: input, send, depth label, depth spin]
//
// The panel does not own any of those children in a semantic sense.
// OverseerPage constructs each one, hands them here, and wires their
// signals back to OverseerWidget. This widget is layout only.
class OverseerBottomPanel : public QWidget {
  Q_OBJECT

public:
  explicit OverseerBottomPanel(QWidget *parent = nullptr);

  // Non-owning. The caller keeps ownership of everything passed in
  // and reparents it into this widget.
  void setSessionHeader(QLabel *header);
  void setAutomationStrip(AutomationStrip *strip);
  void setTranscriptPanel(TranscriptPanel *transcript);

  // The composer controls are created here, because they have no other
  // home. The panel owns them and exposes them so the host can wire
  // their signals.
  QLineEdit *input() const { return m_input; }
  QPushButton *sendButton() const { return m_send; }
  QSpinBox *depthSpin() const { return m_depthSpin; }
  QLabel *depthLabel() const { return m_depthLabel; }

  // One-call convenience for the host: disable the entire composer.
  // The header and strip and transcript stay interactive; only the
  // input path is blocked.
  void setComposerEnabled(bool enabled);

  AutomationStrip *automationStrip() const { return m_strip; }
  TranscriptPanel *transcript() const { return m_transcript; }
  QLabel *sessionHeader() const { return m_header; }

private:
  QLabel *m_header = nullptr;
  AutomationStrip *m_strip = nullptr;
  TranscriptPanel *m_transcript = nullptr;

  QWidget *m_composerRow = nullptr;
  QLineEdit *m_input = nullptr;
  QPushButton *m_send = nullptr;
  QLabel *m_depthLabel = nullptr;
  QSpinBox *m_depthSpin = nullptr;

  QVBoxLayout *m_root = nullptr;
  QVBoxLayout *m_transcriptSlot = nullptr;
};