#ifndef EPISTEME_SPEECHPANEL_H
#define EPISTEME_SPEECHPANEL_H

#include "VoiceCommand.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

class SpeechController;
class VoiceCommandRegistry;

// A non-modal panel listing the registered voice commands. Always
// available regardless of the current editor mode because it is a
// sibling of the main window, not a child of either page.
class SpeechPanel : public QWidget {
  Q_OBJECT

public:
  SpeechPanel(VoiceCommandRegistry *registry, SpeechController *controller,
              QWidget *parent = nullptr);

  // Set the context the commands will act on. MainWindow updates this
  // whenever the current editor changes.
  void setContext(const VoiceContext &context);

  // Refresh the list and the live state labels.
  void refresh();

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onItemSelectionChanged();
  void onRunClicked();
  void onStopClicked();

private:
  QString statusForCommand(VoiceCommand *command) const;
  VoiceCommand *selectedCommand() const;

  VoiceCommandRegistry *m_registry = nullptr;
  SpeechController *m_controller = nullptr;
  VoiceContext m_context;

  QListWidget *m_list = nullptr;
  QLabel *m_description = nullptr;
  QPushButton *m_runButton = nullptr;
  QPushButton *m_stopButton = nullptr;
  QTimer *m_stateTimer = nullptr;
};

#endif // EPISTEME_SPEECHPANEL_H