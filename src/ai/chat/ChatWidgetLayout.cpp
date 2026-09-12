#include "../../../include/ai/chat/ChatWidgetLayout.h"
#include "../../../include/ai/edit/EditSessionWidget.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
constexpr int TranscriptOuterMargin = 14;
constexpr int TranscriptInputHeight = 38;
constexpr int TranscriptSendWidth = 76;
} // namespace

void ChatWidgetLayout::build(QWidget *parent) {
  // Transcript display
  transcript = new QTextEdit(parent);
  transcript->setReadOnly(true);
  transcript->setAcceptRichText(true);
  transcript->setLineWrapMode(QTextEdit::WidgetWidth);
  transcript->setFrameShape(QFrame::NoFrame);
  transcript->setObjectName("transcript");

  QTextDocument *document = transcript->document();
  document->setDocumentMargin(TranscriptOuterMargin);

  // Input field
  input = new QLineEdit(parent);
  input->setPlaceholderText(QObject::tr("Ask the assistant…"));
  input->setClearButtonEnabled(true);
  input->setMinimumHeight(TranscriptInputHeight);
  input->setObjectName("chatInput");

  // Send button
  sendButton = new QPushButton(QObject::tr("Send"), parent);
  sendButton->setMinimumHeight(TranscriptInputHeight);
  sendButton->setMinimumWidth(TranscriptSendWidth);
  sendButton->setDefault(true);
  sendButton->setObjectName("sendButton");

  // Edit mode toggle
  editModeCheckbox = new QCheckBox(QObject::tr("Edit document"), parent);
  editModeCheckbox->setToolTip(
      QObject::tr("Plan and preview document edits before applying them."));
  editModeCheckbox->setObjectName("editModeCheckbox");
  editModeCheckbox->setStyleSheet(
      "QCheckBox::indicator { width: 13px; height: 13px; }");
  
  editSessionWidget = new EditSessionWidget(parent);

  // Content splitter
  contentSplitter = new QSplitter(Qt::Horizontal, parent);
  contentSplitter->setChildrenCollapsible(false);
  contentSplitter->setHandleWidth(6);
  contentSplitter->addWidget(transcript);
  contentSplitter->addWidget(editSessionWidget);
  contentSplitter->setSizes({680, 420});
  contentSplitter->setStretchFactor(0, 1);
  contentSplitter->setStretchFactor(1, 0);
  contentSplitter->setObjectName("contentSplitter");

  // Controls layout
  QWidget *controlsFrame = new QWidget(parent);
  controlsFrame->setObjectName("controlsFrame");
  auto *controlsLayout = new QHBoxLayout(controlsFrame);
  controlsLayout->setContentsMargins(10, 8, 10, 10);
  controlsLayout->setSpacing(8);
  controlsLayout->addWidget(editModeCheckbox);
  controlsLayout->addWidget(input, 1);
  controlsLayout->addWidget(sendButton);

  // Root layout
  rootLayout = new QVBoxLayout(parent);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);
  rootLayout->addWidget(contentSplitter, 1);
  rootLayout->addWidget(controlsFrame);
}