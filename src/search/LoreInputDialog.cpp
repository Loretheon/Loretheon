#include "../../include/text/LoreInputDialog.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QVBoxLayout>

LoreInputDialog::LoreInputDialog(const QPoint &globalPosition,
                                 QWidget *parent)
    : QDialog(parent, Qt::Popup) {
  setObjectName(QStringLiteral("loreInputDialog"));

  m_hint = new QLabel(tr("Search"), this);
  m_hint->setObjectName(QStringLiteral("loreInputHint"));

  m_input = new QLineEdit(this);
  m_input->setObjectName(QStringLiteral("loreInput"));
  m_input->setPlaceholderText(tr("Ask about your notes"));
  m_input->setMinimumWidth(420);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 10, 12, 12);
  layout->setSpacing(6);
  layout->addWidget(m_hint);
  layout->addWidget(m_input);

  connect(m_input, &QLineEdit::returnPressed, this,
          &LoreInputDialog::onAccepted);

  // Position near the cursor without going off-screen.
  QPoint target = globalPosition + QPoint(0, 18);

  QScreen *screen = QGuiApplication::screenAt(globalPosition);
  if (screen) {
    const QRect available = screen->availableGeometry();
    const QSize hint = sizeHint();
    if (target.x() + hint.width() > available.right()) {
      target.setX(available.right() - hint.width() - 8);
    }
    if (target.y() + hint.height() > available.bottom()) {
      target.setY(globalPosition.y() - hint.height() - 8);
    }
  }

  move(target);
}

LoreInputDialog::~LoreInputDialog() = default;

QString LoreInputDialog::request() const {
  return m_input ? m_input->text().trimmed() : QString();
}

void LoreInputDialog::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);
  m_input->setFocus(Qt::OtherFocusReason);
}

void LoreInputDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    event->accept();
    return;
  }

  QDialog::keyPressEvent(event);
}

void LoreInputDialog::onAccepted() {
  if (request().isEmpty()) {
    return;
  }

  accept();
}