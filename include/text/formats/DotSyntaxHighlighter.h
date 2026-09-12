// DotSyntaxHighlighter.h
#ifndef EPISTEME_DOTSYNTAXHIGHLIGHTER_H
#define EPISTEME_DOTSYNTAXHIGHLIGHTER_H

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextDocument>

class DotSyntaxHighlighter : public QSyntaxHighlighter {
  Q_OBJECT

public:
  explicit DotSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
  void highlightBlock(const QString &text) override;

private:
  struct HighlightRule {
    QRegularExpression pattern;
    QTextCharFormat format;
  };

  QVector<HighlightRule> highlightRules;

  QTextCharFormat keywordFormat;
  QTextCharFormat commentFormat;
  QTextCharFormat stringFormat;
  QTextCharFormat numberFormat;
  QTextCharFormat attributeFormat;
  QTextCharFormat operatorFormat;
};

#endif // EPISTEME_DOTSYNTAXHIGHLIGHTER_H