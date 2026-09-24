#include "../../include/assistant/AssistantIcon.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QToolButton>

namespace {

constexpr int kIconSize = 40;
constexpr int kMargin = 24;

} // namespace

AssistantIcon::AssistantIcon(QWidget *parent) : QWidget(parent) {
  // Top-level, frameless, always-on-top, does not take focus. Same
  // flag set as AvatarWidget except that the avatar is added to the
  // stacking order by MainWindow; this icon is not. It is shown only
  // while the main window is minimised.
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                 Qt::NoDropShadowWindowHint |
                 Qt::WindowStaysOnTopHint |
                 Qt::WindowDoesNotAcceptFocus);

  setAttribute(Qt::WA_TranslucentBackground, false);
  setAttribute(Qt::WA_ShowWithoutActivating, true);
  #ifdef Q_OS_LINUX
    setWindowFlags(windowFlags() | Qt::X11BypassWindowManagerHint);
  #endif
  m_button = new QToolButton(this);
  m_button->setObjectName(QStringLiteral("assistantIcon"));
  m_button->setAutoRaise(true);
  m_button->setFixedSize(kIconSize, kIconSize);
  m_button->setIconSize(QSize(kIconSize - 12, kIconSize - 12));
  m_button->setIcon(QIcon::fromTheme(
      QStringLiteral("dialog-information"),
      QIcon(QStringLiteral(":/icons/help-about.png"))));
  m_button->setToolTip(tr("Lore"));

  m_button->move(0, 0);

  setFixedSize(kIconSize, kIconSize);

  connect(m_button, &QToolButton::clicked, this, &AssistantIcon::clicked);

  anchorToScreen();
}

void AssistantIcon::anchorToScreen() {
  QScreen *screen = QGuiApplication::primaryScreen();

  if (!screen) {
    return;
  }

  const QRect available = screen->availableGeometry();

  const int x = available.right() - width() - kMargin;
  const int y = available.top() + kMargin;

  move(x, y);
}