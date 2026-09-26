#pragma once

#include <QWidget>

class QTextEdit;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QComboBox;
class QSplitter;
class QTabWidget;
class QVBoxLayout;

class EditSessionWidget;
class ContextPanel;
class ContextModel;
class HistoryPanel;
class HistoryModel;

struct ChatWidgetLayout {
  QTextEdit *transcript = nullptr;

  QLineEdit *input = nullptr;

  QPushButton *sendButton = nullptr;

  QPushButton *attachButton = nullptr;

  QCheckBox *editModeCheckbox = nullptr;

  QComboBox *editModeCombo = nullptr;

  EditSessionWidget *editSessionWidget = nullptr;

  ContextPanel *contextPanel = nullptr;

  HistoryPanel *historyPanel = nullptr;

  QTabWidget *rightTabs = nullptr;

  QSplitter *contentSplitter = nullptr;

  QVBoxLayout *rootLayout = nullptr;

  void build(QWidget *parent, ContextModel *contextModel,
             HistoryModel *historyModel);
};