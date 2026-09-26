#pragma once

#include <QWidget>

class QToolButton;

// A small always-on-top button that toggles the assistant panel while
// the main window is minimised.
//
// It is a top-level Qt::Tool window with NO QWidget parent. A child
// of MainWindow would become a transient of it, and the window
// manager minimises transients along with their parent. The icon
// must outlive the main window's minimised state, so it has no
// QWidget parent. It is owned by MainWindow via an explicit delete
// in ~MainWindow, the same way AvatarWidget is.
class AssistantIcon : public QWidget {
  Q_OBJECT

public:
  explicit AssistantIcon(QWidget *parent = nullptr);

  // Move the icon to the top-right corner of the primary screen,
  // inset by a small margin. Called when the icon is about to be
  // shown, so the position reflects the current screen geometry
  // rather than the geometry at construction time.
  void anchorToScreen();

  signals:
    void clicked();

private:
  QToolButton *m_button = nullptr;
};