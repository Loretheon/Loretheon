// DiagramToolbar.cpp
#include "DiagramToolbar.h"

#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QToolButton>

DiagramToolbar::DiagramToolbar(QWidget *parent) : QWidget(parent) {
  auto *lay = new QHBoxLayout(this);
  lay->setContentsMargins(4, 2, 4, 2);
  lay->setSpacing(2);

  m_zoomOut   = makeButton(QStringLiteral("\u2212"), tr("Zoom out"));
  m_zoomLabel = makeButton(QStringLiteral("100%"),    tr("Set zoom level"));
  m_zoomIn    = makeButton(QStringLiteral("+"),       tr("Zoom in"));
  m_fit       = makeButton(tr("Fit"),                 tr("Fit to window"));
  m_reset     = makeButton(tr("1:1"),                 tr("Actual size"));

  lay->addWidget(m_zoomOut);
  lay->addWidget(m_zoomLabel);
  lay->addWidget(m_zoomIn);
  lay->addSpacing(8);
  lay->addWidget(m_fit);
  lay->addWidget(m_reset);
  lay->addStretch(1);

  connect(m_zoomOut,   &QToolButton::clicked, this, &DiagramToolbar::zoomOutRequested);
  connect(m_zoomIn,    &QToolButton::clicked, this, &DiagramToolbar::zoomInRequested);
  connect(m_reset,     &QToolButton::clicked, this, &DiagramToolbar::zoomResetRequested);
  connect(m_fit,       &QToolButton::clicked, this, &DiagramToolbar::fitRequested);
  connect(m_zoomLabel, &QToolButton::clicked, this, &DiagramToolbar::openPresetMenu);
}

QToolButton *DiagramToolbar::makeButton(const QString &text, const QString &tip) {
  auto *b = new QToolButton(this);
  b->setText(text);
  b->setToolTip(tip);
  b->setAutoRaise(true);
  b->setFocusPolicy(Qt::NoFocus);
  return b;
}

void DiagramToolbar::openPresetMenu() {
  QMenu menu;
  for (int pct : {25, 50, 75, 100, 150, 200, 400}) {
    QAction *a = menu.addAction(QStringLiteral("%1%").arg(pct));
    a->setData(pct / 100.0);
  }
  menu.addSeparator();
  QAction *fit = menu.addAction(tr("Fit to window"));
  fit->setData(-1.0);

  QAction *chosen = menu.exec(m_zoomLabel->mapToGlobal(
      m_zoomLabel->rect().bottomLeft()));
  if (!chosen) return;

  const qreal v = chosen->data().toReal();
  if (v < 0) emit fitRequested();
  else       emit zoomToRequested(v);
}

void DiagramToolbar::setZoom(qreal zoom) {
  m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(zoom * 100.0)));
}

void DiagramToolbar::setActionsEnabled(bool on) {
  m_zoomIn->setEnabled(on);
  m_zoomOut->setEnabled(on);
  m_zoomLabel->setEnabled(on);
  m_fit->setEnabled(on);
  m_reset->setEnabled(on);
}