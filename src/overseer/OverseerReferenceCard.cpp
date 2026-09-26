#include "../../include/overseer/OverseerReferenceCard.h"

#include "../../include/app/theme/ThemeTokens.h"
#include "ThemeRegistry.h"

#include "../../include/overseer/PathUtils.h"

#include <QClipboard>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

OverseerReferenceCard::OverseerReferenceCard(const QString &relativePath,
                                             const QString &notesRootPath,
                                             QWidget *parent)
    : CardWidget(parent),
      m_relativePath(relativePath),
      m_notesRoot(notesRootPath) {
  setTitle(relativePath);
  setCloseVisible(true);

  m_exists = QFileInfo::exists(absolutePath());

  refreshStatusDot();

  connect(this, &CardWidget::closeRequested, this,
          &OverseerReferenceCard::removed);

  connect(this, &CardWidget::clicked, this, [this]() {
    if (m_exists)
      emit openRequested(m_relativePath);
  });

  buildBody();
}

QString OverseerReferenceCard::absolutePath() const {
  return PathUtils::toAbsolute(m_relativePath, m_notesRoot);
}

void OverseerReferenceCard::refreshStatusDot() {
  setStatusDot(
      ThemeRegistry::instance()
          .color(m_exists ? QStringLiteral("reference.present")
                          : QStringLiteral("reference.missing"))
          .name());
}

void OverseerReferenceCard::changeEvent(QEvent *event) {
  if (event && (event->type() == QEvent::PaletteChange ||
                event->type() == QEvent::StyleChange)) {
    refreshStatusDot();
    update();
  }
  CardWidget::changeEvent(event);
}

void OverseerReferenceCard::populateBody(QVBoxLayout *bodyLayout) {
  m_pathLabel = new QLabel(this);
  m_pathLabel->setObjectName(QStringLiteral("referencePath"));
  m_pathLabel->setWordWrap(true);
  m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_pathLabel->setText(m_exists ? absolutePath()
                                : tr("%1 (missing)").arg(absolutePath()));

  bodyLayout->addWidget(m_pathLabel);
}

QMenu *OverseerReferenceCard::buildContextMenu(QWidget *parent) {
  auto *menu = new QMenu(parent);

  QAction *open = menu->addAction(tr("Open in Workstation"));
  QAction *openNormal = menu->addAction(tr("Open in normal editor"));

  menu->addSeparator();

  QAction *stage = menu->addAction(tr("Copy to session"));

  menu->addSeparator();

  QAction *copyRel = menu->addAction(tr("Copy relative path"));
  QAction *copyAbs = menu->addAction(tr("Copy absolute path"));

  menu->addSeparator();

  QAction *remove = menu->addAction(tr("Remove from Overview"));

  open->setEnabled(m_exists);
  openNormal->setEnabled(m_exists);
  stage->setEnabled(m_exists);

  connect(open, &QAction::triggered, this, [this]() {
    emit openRequested(m_relativePath);
  });

  connect(openNormal, &QAction::triggered, this, [this]() {
    emit openInNormalEditorRequested(m_relativePath);
  });

  connect(stage, &QAction::triggered, this, [this]() {
    emit stageRequested(m_relativePath);
  });

  connect(copyRel, &QAction::triggered, this, [this]() {
    QGuiApplication::clipboard()->setText(m_relativePath);
  });

  connect(copyAbs, &QAction::triggered, this, [this]() {
    QGuiApplication::clipboard()->setText(absolutePath());
  });

  connect(remove, &QAction::triggered, this,
          &OverseerReferenceCard::removed);

  return menu;
}