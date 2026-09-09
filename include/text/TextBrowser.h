#ifndef EPISTEME_TEXTBROWSER_H
#define EPISTEME_TEXTBROWSER_H
#include <QTextBrowser>

class TextBrowser : public QTextBrowser {
  Q_OBJECT

public:
  explicit TextBrowser(QWidget *parent = nullptr);
};

#endif // EPISTEME_TEXTBROWSER_H