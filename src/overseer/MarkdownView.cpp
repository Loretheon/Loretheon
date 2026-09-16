#include "../../include/overseer/MarkdownView.h"

#include <QAbstractTextDocumentLayout>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextDocument>

MarkdownView::MarkdownView(QWidget *parent) : TextBrowser(parent) {
  setObjectName(QStringLiteral("markdownView"));

  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  setTextInteractionFlags(Qt::TextSelectableByMouse |
                          Qt::TextSelectableByKeyboard |
                          Qt::LinksAccessibleByMouse);

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  applyDocumentStyle();

  if (!m_connectedToDocumentLayout) {
    connect(document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged, this,
            [this](const QSizeF &) { updateHeight(); });
    m_connectedToDocumentLayout = true;
  }
}

void MarkdownView::applyDocumentStyle() {
  QTextDocument *doc = document();

  doc->setDocumentMargin(0);

  doc->setDefaultStyleSheet(QStringLiteral(
      "pre { background: rgba(128,128,128,0.12); padding: 6px; }"
      "code { font-family: monospace; }"
      "blockquote { border-left: 3px solid rgba(128,128,128,0.4);"
      "             margin-left: 0; padding-left: 8px; }"
      "h1, h2, h3, h4, h5, h6 { margin-top: 6px; margin-bottom: 2px; }"));
}

void MarkdownView::setMarkdownText(const QString &markdown) {
  if (markdown.isEmpty()) {
    clear();
    updateHeight();
    return;
  }

  QTextDocument *doc = document();

  doc->setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);

  applyDocumentStyle();

  updateHeight();
}

void MarkdownView::setPlainTextBody(const QString &text) {
  if (text.isEmpty()) {
    clear();
    updateHeight();
    return;
  }

  setPlainText(text);
  updateHeight();
}

void MarkdownView::resizeEvent(QResizeEvent *event) {
  TextBrowser::resizeEvent(event);
  updateHeight();
}

void MarkdownView::showEvent(QShowEvent *event) {
  TextBrowser::showEvent(event);
  updateHeight();
}

void MarkdownView::updateHeight() {
  document()->setTextWidth(viewport()->width());

  const qreal docHeight = document()->size().height();

  const int frame = frameWidth() * 2;
  const int newHeight = qMax(1, static_cast<int>(qCeil(docHeight)) + frame);

  setFixedHeight(newHeight);
}