#include "../../include/overseer/CardWidget.h"

#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

CardWidget::CardWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("cardWidget"));
  setAttribute(Qt::WA_StyledBackground, true);
  setMouseTracking(true);

  m_header = new QWidget(this);
  m_header->setObjectName(QStringLiteral("cardWidgetHeader"));
  m_header->setAttribute(Qt::WA_StyledBackground, true);

  m_leadingIcon = new QLabel(m_header);
  m_leadingIcon->setObjectName(QStringLiteral("cardWidgetIcon"));
  m_leadingIcon->setFixedWidth(18);
  m_leadingIcon->hide();

  m_statusDot = new QLabel(m_header);
  m_statusDot->setObjectName(QStringLiteral("cardWidgetDot"));
  m_statusDot->setFixedSize(8, 8);
  m_statusDot->hide();

  m_titleLabel = new QLabel(m_header);
  m_titleLabel->setObjectName(QStringLiteral("cardWidgetTitle"));

  m_subtitleLabel = new QLabel(m_header);
  m_subtitleLabel->setObjectName(QStringLiteral("cardWidgetSubtitle"));

  m_closeButton = new QToolButton(m_header);
  m_closeButton->setObjectName(QStringLiteral("cardWidgetClose"));
  m_closeButton->setText(QStringLiteral("\u2715"));
  m_closeButton->setAutoRaise(true);
  m_closeButton->setCursor(Qt::PointingHandCursor);
  m_closeButton->hide();

  m_headerLayout = new QHBoxLayout(m_header);
  m_headerLayout->setContentsMargins(8, 4, 4, 4);
  m_headerLayout->setSpacing(6);
  m_headerLayout->addWidget(m_leadingIcon);
  m_headerLayout->addWidget(m_statusDot);
  m_headerLayout->addWidget(m_titleLabel);
  m_headerLayout->addWidget(m_subtitleLabel);
  m_headerLayout->addStretch(1);
  m_headerLayout->addWidget(m_closeButton);

  m_body = new QWidget(this);
  m_bodyLayout = new QVBoxLayout(m_body);
  m_bodyLayout->setContentsMargins(10, 6, 10, 10);
  m_bodyLayout->setSpacing(4);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);
  root->addWidget(m_header);
  root->addWidget(m_body, 1);

  connect(m_closeButton, &QToolButton::clicked, this,
          &CardWidget::closeRequested);
}

CardWidget::~CardWidget() = default;

void CardWidget::buildBody() {
  if (m_bodyBuilt)
    return;

  m_bodyBuilt = true;

  populateBody(m_bodyLayout);
}

void CardWidget::setTitle(const QString &title) {
  m_titleLabel->setText(title);
}

void CardWidget::setSubtitle(const QString &subtitle) {
  m_subtitleLabel->setText(subtitle);
  m_subtitleLabel->setVisible(!subtitle.isEmpty());
}

void CardWidget::setStatusDot(const QString &colorHex) {
  m_statusDot->setStyleSheet(
      QStringLiteral("background:%1;border-radius:4px;").arg(colorHex));
  m_statusDot->show();
}

void CardWidget::clearStatusDot() { m_statusDot->hide(); }

void CardWidget::setLeadingIcon(const QString &iconName) {
  m_leadingIcon->setText(iconName);
  m_leadingIcon->setVisible(!iconName.isEmpty());
}

void CardWidget::setCloseVisible(bool visible) {
  m_closeButton->setVisible(visible);
}

void CardWidget::setFocusedCard(bool focused) {
  if (m_focused == focused)
    return;

  m_focused = focused;
  onFocusedChanged(focused);

  setProperty("focused", focused);
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void CardWidget::addHeaderAction(QToolButton *button) {
  if (!button)
    return;
  m_headerLayout->insertWidget(m_headerLayout->count() - 1, button);
}

QMenu *CardWidget::buildContextMenu(QWidget *parent) {
  Q_UNUSED(parent);
  return nullptr;
}

void CardWidget::onFocusedChanged(bool focused) { Q_UNUSED(focused); }
void CardWidget::onHoverChanged(bool hovered) { Q_UNUSED(hovered); }

void CardWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton)
    emit clicked();
  QWidget::mousePressEvent(event);
}

void CardWidget::mouseReleaseEvent(QMouseEvent *event) {
  QWidget::mouseReleaseEvent(event);
}

void CardWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  emit doubleClicked();
  QWidget::mouseDoubleClickEvent(event);
}

void CardWidget::enterEvent(QEnterEvent *event) {
  m_hovered = true;
  onHoverChanged(true);
  setProperty("hovered", true);
  style()->unpolish(this);
  style()->polish(this);
  QWidget::enterEvent(event);
}

void CardWidget::leaveEvent(QEvent *event) {
  m_hovered = false;
  onHoverChanged(false);
  setProperty("hovered", false);
  style()->unpolish(this);
  style()->polish(this);
  QWidget::leaveEvent(event);
}

void CardWidget::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = buildContextMenu(this);

  if (!menu)
    return;

  menu->exec(event->globalPos());
  menu->deleteLater();
}

void CardWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QColor bg = palette().color(QPalette::Window);
  QColor border = palette().color(QPalette::Mid);
  border.setAlpha(140);

  painter.setBrush(bg);
  painter.setPen(border);

  painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
}