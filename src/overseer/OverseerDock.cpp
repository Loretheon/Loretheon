#include "../../include/overseer/OverseerDock.h"

#include "../../include/overseer/OverseerWidget.h"

#include <QWidget>

OverseerDock::OverseerDock(InferenceService *inferenceService, QWidget *parent)
    : QDockWidget(tr("Overseer"), parent) {
  setObjectName(QStringLiteral("overseerDock"));
  setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

  m_overseer = new OverseerWidget(inferenceService, this);
  setWidget(m_overseer);
}