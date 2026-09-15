#pragma once

#include "HistoryModel.h"

#include <QWidget>

class QLabel;
class QScrollArea;
class QVBoxLayout;

class HistoryPanel : public QWidget {
  Q_OBJECT

public:
  explicit HistoryPanel(HistoryModel *model, QWidget *parent = nullptr);

private slots:
  void rebuild();

private:
  HistoryModel *m_model = nullptr;

  QLabel *m_summary = nullptr;
  QScrollArea *m_scrollArea = nullptr;
  QWidget *m_content = nullptr;
  QVBoxLayout *m_contentLayout = nullptr;
};