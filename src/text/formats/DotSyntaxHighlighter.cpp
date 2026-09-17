// DotSyntaxHighlighter.cpp
#include "DotSyntaxHighlighter.h"
#include <QTextCharFormat>

DotSyntaxHighlighter::DotSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  // Keywords
  keywordFormat.setForeground(Qt::darkBlue);
  keywordFormat.setFontWeight(QFont::Bold);

  QStringList keywords = {
      "digraph", "graph", "subgraph", "node", "edge", "strict"};
  for (const QString &keyword : keywords) {
    HighlightRule rule;
    rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
    rule.format = keywordFormat;
    highlightRules.append(rule);
  }

  // Comments
  commentFormat.setForeground(Qt::darkGreen);
  commentFormat.setFontItalic(true);
  HighlightRule commentRule;
  commentRule.pattern = QRegularExpression("//.*");
  commentRule.format = commentFormat;
  highlightRules.append(commentRule);

  HighlightRule multilineCommentRule;
  multilineCommentRule.pattern = QRegularExpression("/\\*.*\\*/");
  multilineCommentRule.format = commentFormat;
  highlightRules.append(multilineCommentRule);

  // Strings
  stringFormat.setForeground(Qt::darkRed);
  HighlightRule stringRule;
  stringRule.pattern = QRegularExpression("\".*?\"");
  stringRule.format = stringFormat;
  highlightRules.append(stringRule);

  // Numbers
  numberFormat.setForeground(Qt::darkCyan);
  HighlightRule numberRule;
  numberRule.pattern = QRegularExpression("\\b\\d+(\\.\\d+)?\\b");
  numberRule.format = numberFormat;
  highlightRules.append(numberRule);

  // Attributes (label, color, shape, etc.)
  attributeFormat.setForeground(Qt::darkMagenta);
  QStringList attributes = {"label",    "color",  "shape",   "style",
                            "fontsize", "width",  "height",  "penwidth",
                            "fillcolor"};
  for (const QString &attr : attributes) {
    HighlightRule rule;
    rule.pattern = QRegularExpression("\\b" + attr + "\\b");
    rule.format = attributeFormat;
    highlightRules.append(rule);
  }

  // Operators
  operatorFormat.setForeground(Qt::darkYellow);
  HighlightRule opRule;
  opRule.pattern = QRegularExpression("[\\-\\>\\=\\[\\]\\{\\}\\,\\;]");
  opRule.format = operatorFormat;
  highlightRules.append(opRule);
}

void DotSyntaxHighlighter::highlightBlock(const QString &text) {
  for (const HighlightRule &rule : highlightRules) {
    QRegularExpressionMatchIterator matchIterator =
        rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }

  setCurrentBlockState(0);
}