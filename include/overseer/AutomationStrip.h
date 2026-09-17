#pragma once

#include <QWidget>

#include "SessionSettings.h"

class QCheckBox;
class QLabel;
class QPushButton;

// A compact strip of automation toggles, shown at the top of the
// Overseer center column. Three states:
//
//   - normal  : three checkboxes visible (Automatic, Memory, Edits)
//   - automatic: checkboxes collapsed into a single indicator plus an
//                "Off" button, since the general mode forces the others
//                on.
//
// The strip is purely a view over SessionSettings; it emits a signal
// whenever the user changes something and the owner persists it.
class AutomationStrip : public QWidget {
  Q_OBJECT

public:
  explicit AutomationStrip(QWidget *parent = nullptr);

  void setSettings(const SessionSettings &settings);
  SessionSettings settings() const { return m_settings; }

  void setEnabledState(bool enabled);

  signals:
    void settingsChanged(const SessionSettings &settings);

private slots:
  void onAutomaticToggled(bool on);
  void onMemoryToggled(bool on);
  void onEditsToggled(bool on);
  void onOffClicked();

private:
  void applyToUi();
  void emitChanged();

  SessionSettings m_settings;

  QCheckBox *m_automaticCheck = nullptr;
  QCheckBox *m_memoryCheck = nullptr;
  QCheckBox *m_editsCheck = nullptr;

  QWidget *m_automaticIndicator = nullptr;
  QLabel *m_automaticLabel = nullptr;
  QPushButton *m_offButton = nullptr;

  bool m_updating = false;
};