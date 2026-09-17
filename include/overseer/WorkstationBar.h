#pragma once

#include <QWidget>

class Workstation;
class WorkstationWindow;

class QHBoxLayout;
class QScrollArea;
class QToolButton;

class WorkstationBar : public QWidget {
  Q_OBJECT

public:
  explicit WorkstationBar(Workstation *workstation, QWidget *parent = nullptr);

private slots:
  void rebuild();

private:
  Workstation *m_workstation = nullptr;

  QScrollArea *m_scroll = nullptr;
  QWidget *m_host = nullptr;
  QHBoxLayout *m_hostLayout = nullptr;
};