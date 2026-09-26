#ifndef CUSTOMTITLEBAR_H
#define CUSTOMTITLEBAR_H

#include <QWidget>

class QLabel;
class QToolButton;

class CustomTitleBar : public QWidget {
  Q_OBJECT

public:
  explicit CustomTitleBar(QWidget *parent = nullptr);

  void setTitle(const QString &title);

  signals:
    void minimizeRequested();
  void maximizeRequested();
  void closeRequested();

private:
  QLabel *m_titleLabel = nullptr;
  QToolButton *m_minimizeBtn = nullptr;
  QToolButton *m_maximizeBtn = nullptr;
  QToolButton *m_closeBtn = nullptr;
};

#endif // CUSTOMTITLEBAR_H