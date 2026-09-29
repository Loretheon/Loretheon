#include "../../include/overseer/OverseerBottomPanel.h"

#include "AutomationStrip.h"
#include "TranscriptPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

OverseerBottomPanel::OverseerBottomPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("overseerBottomPanel"));

  m_root = new QVBoxLayout(this);
  m_root->setContentsMargins(6, 6, 6, 6);
  m_root->setSpacing(6);

  // The session header slot. The caller will hand us an existing
  // QLabel; until then the layout has a placeholder so the panel does
  // not jump when the real label arrives.
  m_root->addWidget(new QLabel(tr("No session"), this), 0);

  // The composer row. Input, send, depth label, depth spin. Created
  // here because there is nowhere else for them to live; the host
  // reads them back via the accessors above.
  m_composerRow = new QWidget(this);
  auto *composerLayout = new QHBoxLayout(m_composerRow);
  composerLayout->setContentsMargins(0, 0, 0, 0);
  composerLayout->setSpacing(6);

  m_input = new QLineEdit(m_composerRow);
  m_input->setPlaceholderText(tr("Describe a task…"));

  m_send = new QPushButton(tr("Send"), m_composerRow);

  m_depthLabel = new QLabel(tr("Tool depth:"), m_composerRow);

  m_depthSpin = new QSpinBox(m_composerRow);
  m_depthSpin->setRange(1, 100000);

  composerLayout->addWidget(m_input, 1);
  composerLayout->addWidget(m_send);
  composerLayout->addSpacing(12);
  composerLayout->addWidget(m_depthLabel);
  composerLayout->addWidget(m_depthSpin);

  m_root->addWidget(m_composerRow, 0);
}

void OverseerBottomPanel::setSessionHeader(QLabel *header) {
  if (m_header == header)
    return;

  // Remove the current occupant of row 0 and install the new one.
  if (QLayoutItem *item = m_root->itemAt(0)) {
    if (QWidget *w = item->widget()) {
      if (w != header) {
        m_root->removeWidget(w);
        w->setParent(nullptr);
      }
    }
  }

  m_header = header;

  if (m_header) {
    m_header->setParent(this);
    m_root->insertWidget(0, m_header, 0);
    m_header->show();
  }
}

void OverseerBottomPanel::setAutomationStrip(AutomationStrip *strip) {
  if (m_strip == strip)
    return;

  if (m_strip) {
    m_root->removeWidget(m_strip);
    m_strip->setParent(nullptr);
  }

  m_strip = strip;

  if (m_strip) {
    m_strip->setParent(this);
    m_root->insertWidget(1, m_strip, 0);
    m_strip->show();
  }
}

void OverseerBottomPanel::setTranscriptPanel(TranscriptPanel *transcript) {
  if (m_transcript == transcript)
    return;

  if (m_transcript) {
    m_root->removeWidget(m_transcript);
    m_transcript->setParent(nullptr);
  }

  m_transcript = transcript;

  if (m_transcript) {
    m_transcript->setParent(this);
    // Between the automation strip (row 0 or 1) and the composer row
    // (last row). insertWidget with an explicit index keeps the
    // ordering stable regardless of what the caller passed in first.
    const int insertIndex = m_root->indexOf(m_composerRow);
    m_root->insertWidget(insertIndex < 0 ? m_root->count() - 1 : insertIndex,
                         m_transcript, 1);
    m_transcript->show();
  }
}

void OverseerBottomPanel::setComposerEnabled(bool enabled) {
  if (m_input)
    m_input->setEnabled(enabled);
  if (m_send)
    m_send->setEnabled(enabled);
  if (m_depthSpin)
    m_depthSpin->setEnabled(enabled);
  if (m_depthLabel)
    m_depthLabel->setEnabled(enabled);
}