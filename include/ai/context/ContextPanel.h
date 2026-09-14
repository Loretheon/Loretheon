#pragma once

#include "ContextModel.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class ContextPanel : public QWidget {
  Q_OBJECT

public:
  explicit ContextPanel(ContextModel *model, QWidget *parent = nullptr);

private slots:
  void rebuild();

  void onItemChanged(QTreeWidgetItem *item, int column);

private:
  ContextModel *m_model = nullptr;

  QLabel *m_summary = nullptr;
  QTreeWidget *m_tree = nullptr;
  QPushButton *m_includeAllButton = nullptr;
  QPushButton *m_excludeAllButton = nullptr;
  QPushButton *m_refreshButton = nullptr;

  bool m_rebuilding = false;
};