#pragma once

#include <QWidget>

class QTextEdit;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QSplitter;
class QTabWidget;
class QVBoxLayout;

class EditSessionWidget;
class ContextPanel;
class ContextModel;

struct ChatWidgetLayout {
  QTextEdit *transcript = nullptr;

  QLineEdit *input = nullptr;

  QPushButton *sendButton = nullptr;

  QCheckBox *editModeCheckbox = nullptr;

  EditSessionWidget *editSessionWidget = nullptr;

  ContextPanel *contextPanel = nullptr;

  QTabWidget *rightTabs = nullptr;

  QSplitter *contentSplitter = nullptr;

  QVBoxLayout *rootLayout = nullptr;

  void build(QWidget *parent, ContextModel *contextModel);
};