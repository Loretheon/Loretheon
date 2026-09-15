#include "../../../include/ai/chat/ChatWidgetLayout.h"
#include "../../../include/ai/context/ContextModel.h"
#include "../../../include/ai/context/ContextPanel.h"
#include "../../../include/ai/edit/EditSessionWidget.h"
#include "../../../include/ai/history/HistoryModel.h"
#include "../../../include/ai/history/HistoryPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
constexpr int TranscriptOuterMargin = 14;
constexpr int TranscriptInputHeight = 38;
constexpr int TranscriptSendWidth = 76;
} // namespace

void ChatWidgetLayout::build(QWidget *parent, ContextModel *contextModel,
                             HistoryModel *historyModel) {
  transcript = new QTextEdit(parent);
  transcript->setReadOnly(true);
  transcript->setAcceptRichText(true);
  transcript->setLineWrapMode(QTextEdit::WidgetWidth);
  transcript->setFrameShape(QFrame::NoFrame);
  transcript->setObjectName("transcript");

  QTextDocument *document = transcript->document();
  document->setDocumentMargin(TranscriptOuterMargin);

  input = new QLineEdit(parent);
  input->setPlaceholderText(QObject::tr("Ask the assistant…"));
  input->setClearButtonEnabled(true);
  input->setMinimumHeight(TranscriptInputHeight);
  input->setObjectName("chatInput");

  sendButton = new QPushButton(QObject::tr("Send"), parent);
  sendButton->setMinimumHeight(TranscriptInputHeight);
  sendButton->setMinimumWidth(TranscriptSendWidth);
  sendButton->setDefault(true);
  sendButton->setObjectName("sendButton");

  editModeCheckbox = new QCheckBox(QObject::tr("Edit document"), parent);
  editModeCheckbox->setToolTip(
      QObject::tr("Plan and preview document edits before applying them."));
  editModeCheckbox->setObjectName("editModeCheckbox");
  editModeCheckbox->setStyleSheet(
      "QCheckBox::indicator { width: 13px; height: 13px; }");

  editModeCombo = new QComboBox(parent);
  editModeCombo->addItem(QObject::tr("Scoped edit"));
  editModeCombo->addItem(QObject::tr("Whole-file rewrite"));
  editModeCombo->setToolTip(
      QObject::tr("Scoped: target individual sections. "
                  "Whole-file: rewrite the entire document as one edit."));
  editModeCombo->setObjectName("editModeCombo");

  editSessionWidget = new EditSessionWidget(parent);

  contextPanel = new ContextPanel(contextModel, parent);

  historyPanel = new HistoryPanel(historyModel, parent);

  rightTabs = new QTabWidget(parent);
  rightTabs->addTab(editSessionWidget, QObject::tr("Edits"));
  rightTabs->addTab(contextPanel, QObject::tr("Context"));
  rightTabs->addTab(historyPanel, QObject::tr("History"));

  contentSplitter = new QSplitter(Qt::Horizontal, parent);
  contentSplitter->setChildrenCollapsible(false);
  contentSplitter->setHandleWidth(6);
  contentSplitter->addWidget(transcript);
  contentSplitter->addWidget(rightTabs);
  contentSplitter->setSizes({680, 420});
  contentSplitter->setStretchFactor(0, 1);
  contentSplitter->setStretchFactor(1, 0);
  contentSplitter->setObjectName("contentSplitter");

  QWidget *controlsFrame = new QWidget(parent);
  controlsFrame->setObjectName("controlsFrame");
  auto *controlsLayout = new QHBoxLayout(controlsFrame);
  controlsLayout->setContentsMargins(10, 8, 10, 10);
  controlsLayout->setSpacing(8);
  controlsLayout->addWidget(editModeCheckbox);
  controlsLayout->addWidget(editModeCombo);
  controlsLayout->addWidget(input, 1);
  controlsLayout->addWidget(sendButton);

  rootLayout = new QVBoxLayout(parent);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);
  rootLayout->addWidget(contentSplitter, 1);
  rootLayout->addWidget(controlsFrame);
}