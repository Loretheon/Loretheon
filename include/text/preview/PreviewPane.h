#pragma once

#include <QPlainTextEdit>
#include <QVector>

class QTextDocument;

class PreviewPane : public QPlainTextEdit {
  Q_OBJECT

public:
  struct Highlight {
    int start = -1;
    int end = -1;
    QColor color;
    bool strikethrough = false;
  };

  explicit PreviewPane(QWidget *parent = nullptr);

  void setShadowText(const QString &text);
  void setHighlights(const QVector<Highlight> &highlights);
  void clearAll();

private:
  QVector<Highlight> m_highlights;
};