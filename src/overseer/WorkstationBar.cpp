#include "../../include/overseer/WorkstationBar.h"

#include "../../include/overseer/Workstation.h"
#include "../../include/overseer/WorkstationWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMenu>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>

WorkstationBar::WorkstationBar(Workstation *workstation, QWidget *parent)
    : QWidget(parent), m_workstation(workstation) {
  setObjectName(QStringLiteral("workstationBar"));
  setFixedHeight(30);

  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(4, 2, 4, 2);
  root->setSpacing(0);

  m_scroll = new QScrollArea(this);
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_host = new QWidget;
  m_hostLayout = new QHBoxLayout(m_host);
  m_hostLayout->setContentsMargins(0, 0, 0, 0);
  m_hostLayout->setSpacing(4);
  m_hostLayout->setAlignment(Qt::AlignLeft);
  m_hostLayout->addStretch(1);

  m_scroll->setWidget(m_host);
  root->addWidget(m_scroll);

  if (m_workstation) {
    connect(m_workstation, &Workstation::windowListChanged, this,
            &WorkstationBar::rebuild);

    connect(m_workstation, &Workstation::currentFileChanged, this,
            [this](const QString &) { rebuild(); });
  }

  rebuild();
}

void WorkstationBar::rebuild() {
  while (m_hostLayout->count() > 1) {
    QLayoutItem *item = m_hostLayout->takeAt(0);

    if (!item)
      break;

    if (QWidget *w = item->widget())
      w->deleteLater();

    delete item;
  }

  if (!m_workstation)
    return;

  const QString focusedPath =
      m_workstation->focusedWindow()
          ? m_workstation->focusedWindow()->filePath()
          : QString();

  const auto windows = m_workstation->windows();

  int index = 0;
  bool lastWasTiled = true;
  bool firstEmitted = false;

  auto addSeparator = [&]() {
    auto *sep = new QLabel(QStringLiteral("·"), m_host);
    sep->setObjectName(QStringLiteral("workstationBarSeparator"));
    m_hostLayout->insertWidget(index, sep);
    ++index;
  };

  for (WorkstationWindow *w : windows) {
    if (!w)
      continue;

    const bool tiled = w->mode() == WorkstationWindow::Mode::Tiled;

    if (firstEmitted && tiled != lastWasTiled)
      addSeparator();

    auto *button = new QToolButton(m_host);
    button->setText(w->displayName());
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setCheckable(true);
    button->setChecked(w->filePath() == focusedPath);
    button->setAutoRaise(true);
    button->setProperty("focused", w->filePath() == focusedPath);
    button->setProperty("mode", tiled ? QStringLiteral("tiled")
                                      : QStringLiteral("floating"));
    button->setToolTip(w->filePath());
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(button, &QToolButton::clicked, this, [this, w]() {
      if (!m_workstation)
        return;
      m_workstation->focusWindow(w);
    });

    connect(button, &QToolButton::customContextMenuRequested, this,
            [this, w, button](const QPoint &pos) {
              if (!m_workstation)
                return;

              QMenu menu(button);

              QAction *close = menu.addAction(tr("Close"));
              QAction *tile = menu.addAction(tr("Tile"));
              QAction *floatAct = menu.addAction(tr("Float"));

              QAction *chosen = menu.exec(button->mapToGlobal(pos));

              if (chosen == close) {
                m_workstation->closeFile(w->filePath());
              } else if (chosen == tile) {
                w->setMode(WorkstationWindow::Mode::Tiled);
              } else if (chosen == floatAct) {
                w->setMode(WorkstationWindow::Mode::Floating);
              }
            });

    m_hostLayout->insertWidget(index, button);
    ++index;

    lastWasTiled = tiled;
    firstEmitted = true;
  }
}