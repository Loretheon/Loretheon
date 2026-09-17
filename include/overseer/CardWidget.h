#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QToolButton;

class CardWidget : public QWidget {
  Q_OBJECT

public:
  explicit CardWidget(QWidget *parent = nullptr);
  ~CardWidget() override;

  void setTitle(const QString &title);
  void setSubtitle(const QString &subtitle);
  void setStatusDot(const QString &colorHex);
  void clearStatusDot();

  void setLeadingIcon(const QString &iconName);

  void setCloseVisible(bool visible);

  bool isFocusedCard() const { return m_focused; }
  void setFocusedCard(bool focused);

signals:
  void clicked();
  void doubleClicked();
  void closeRequested();

protected:
  // Subclasses call buildBody() at the *end* of their own constructor.
  // Never call from CardWidget's constructor: the vtable is still
  // pointing at CardWidget at that point.
  void buildBody();

  // Override to add widgets into the body layout.
  virtual void populateBody(QVBoxLayout *bodyLayout) = 0;

  // Override to provide a context menu. Return nullptr for none.
  virtual QMenu *buildContextMenu(QWidget *parent);

  virtual void onFocusedChanged(bool focused);
  virtual void onHoverChanged(bool hovered);

  QLabel *titleLabel() const { return m_titleLabel; }
  QLabel *subtitleLabel() const { return m_subtitleLabel; }
  QVBoxLayout *bodyLayout() const { return m_bodyLayout; }

  void addHeaderAction(QToolButton *button);

  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  QWidget *m_header = nullptr;
  QHBoxLayout *m_headerLayout = nullptr;
  QLabel *m_leadingIcon = nullptr;
  QLabel *m_statusDot = nullptr;
  QLabel *m_titleLabel = nullptr;
  QLabel *m_subtitleLabel = nullptr;
  QToolButton *m_closeButton = nullptr;
  QWidget *m_body = nullptr;
  QVBoxLayout *m_bodyLayout = nullptr;

  bool m_focused = false;
  bool m_hovered = false;
  bool m_bodyBuilt = false;
};