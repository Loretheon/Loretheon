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

  setFocusPolicy(Qt::NoFocus);

  setTextInteractionFlags(Qt::TextSelectableByMouse |
                          Qt::TextSelectableByKeyboard |
                          Qt::LinksAccessibleByMouse);

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  QPalette pal = palette();
  pal.setColor(QPalette::Base, Qt::transparent);
  pal.setColor(QPalette::Window, Qt::transparent);
  setPalette(pal);

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
      "body { color: #E8F0F8; font-size: 11pt; line-height: 145%; }"
      "p { margin-top: 2px; margin-bottom: 10px; }"

      "h1, h2, h3, h4, h5, h6 {"
      "  color: #E8F0F8;"
      "  font-weight: 600;"
      "  margin-top: 16px;"
      "  margin-bottom: 6px; }"
      "h1 { font-size: 15pt; }"
      "h2 { font-size: 13pt; }"
      "h3 { font-size: 12pt; }"
      "h4, h5, h6 { font-size: 11pt; }"

      "a { color: #E69F00; text-decoration: none; }"
      "a:hover { text-decoration: underline; }"

      "code {"
      "  font-family: 'JetBrains Mono','Fira Code','Courier New',monospace;"
      "  font-size: 10pt;"
      "  background: rgba(255,255,255,0.06);"
      "  color: #F5C44D;"
      "  padding: 1px 5px; }"

      "pre {"
      "  background: rgba(0,0,0,0.28);"
      "  border-left: 2px solid #E69F00;"
      "  padding: 10px 14px;"
      "  margin: 8px 0;"
      "  color: #E8F0F8; }"
      "pre code {"
      "  background: transparent;"
      "  padding: 0;"
      "  color: #E8F0F8; }"

      "blockquote {"
      "  border-left: 3px solid #E69F00;"
      "  color: #A8BED0;"
      "  margin-left: 0;"
      "  margin-right: 0;"
      "  padding-left: 12px;"
      "  font-style: italic; }"

      "ul, ol { margin-top: 2px; margin-bottom: 8px; }"
      "li { margin-bottom: 4px; }"

      "hr {"
      "  border: none;"
      "  border-top: 1px solid #1F3A5F;"
      "  margin: 14px 0; }"

      "table {"
      "  border-collapse: collapse;"
      "  margin: 10px 0; }"
      "th, td {"
      "  border: 1px solid #1F3A5F;"
      "  padding: 5px 10px; }"
      "th {"
      "  background-color: #162545;"
      "  color: #A8BED0;"
      "  font-weight: 600; }"));
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