#pragma once

#include <QWidget>

class QTextEdit;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QSplitter;
class QVBoxLayout;

class EditSessionWidget;

struct ChatWidgetLayout {
  QTextEdit *transcript = nullptr;

  QLineEdit *input = nullptr;

  QPushButton *sendButton = nullptr;

  QCheckBox *editModeCheckbox = nullptr;

  EditSessionWidget *editSessionWidget = nullptr;

  QSplitter *contentSplitter = nullptr;

  QVBoxLayout *rootLayout = nullptr;

  void build(QWidget *parent);
};