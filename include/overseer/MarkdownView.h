#pragma once

#include "TextBrowser.h"

#include <QString>

class MarkdownView : public TextBrowser {
  Q_OBJECT

public:
  explicit MarkdownView(QWidget *parent = nullptr);

  void setMarkdownText(const QString &markdown);
  void setPlainTextBody(const QString &text);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void applyDocumentStyle();
  void updateHeight();
  bool m_connectedToDocumentLayout = false;
};