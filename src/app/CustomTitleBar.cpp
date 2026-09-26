#include "CustomTitleBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QStyle>
#include <QApplication>

CustomTitleBar::CustomTitleBar(QWidget *parent) : QWidget(parent) {
  setMinimumHeight(32);
  setMaximumHeight(32);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 0, 0, 0);
  layout->setSpacing(0);

  m_titleLabel = new QLabel(this);
  m_titleLabel->setText("Lore");
  m_titleLabel->setStyleSheet("color: white; font-weight: bold;");

  layout->addWidget(m_titleLabel);
  layout->addStretch();

  m_minimizeBtn = new QToolButton(this);
  m_minimizeBtn->setIcon(QApplication::style()->standardIcon(
      QStyle::SP_TitleBarMinButton));
  m_minimizeBtn->setFixedSize(32, 32);
  m_minimizeBtn->setFocusPolicy(Qt::NoFocus);
  layout->addWidget(m_minimizeBtn);

  m_maximizeBtn = new QToolButton(this);
  m_maximizeBtn->setIcon(QApplication::style()->standardIcon(
      QStyle::SP_TitleBarMaxButton));
  m_maximizeBtn->setFixedSize(32, 32);
  m_maximizeBtn->setFocusPolicy(Qt::NoFocus);
  layout->addWidget(m_maximizeBtn);

  m_closeBtn = new QToolButton(this);
  m_closeBtn->setIcon(QApplication::style()->standardIcon(
      QStyle::SP_TitleBarCloseButton));
  m_closeBtn->setFixedSize(32, 32);
  m_closeBtn->setFocusPolicy(Qt::NoFocus);
  layout->addWidget(m_closeBtn);

  connect(m_minimizeBtn, &QToolButton::clicked, this,
          &CustomTitleBar::minimizeRequested);
  connect(m_maximizeBtn, &QToolButton::clicked, this,
          &CustomTitleBar::maximizeRequested);
  connect(m_closeBtn, &QToolButton::clicked, this,
          &CustomTitleBar::closeRequested);
}

void CustomTitleBar::setTitle(const QString &title) {
  m_titleLabel->setText(title);
}