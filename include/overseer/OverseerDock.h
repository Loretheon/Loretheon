#pragma once

#include <QDockWidget>

class InferenceService;
class OverseerWidget;

class OverseerDock : public QDockWidget {
  Q_OBJECT

public:
  explicit OverseerDock(InferenceService *inferenceService,
                        QWidget *parent = nullptr);

  OverseerWidget *overseerWidget() const { return m_overseer; }

private:
  OverseerWidget *m_overseer = nullptr;
};