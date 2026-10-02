#pragma once

#include "ChatNode.h"

#include <QHash>
#include <QWidget>

class ChatTree;
class MarkdownView;

class QHBoxLayout;
class QLabel;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

class ChatNodeWidget : public QWidget {
  Q_OBJECT

public:
  explicit ChatNodeWidget(const ChatNode &node, ChatTree *tree,
                          QWidget *parent = nullptr);

  QString nodeId() const { return m_id; }

  void setTree(ChatTree *tree) { m_tree = tree; }

  void updateNode(const ChatNode &node);

  signals:
    void abortRequested(const QString &jobId);

private:
  void applyStructure();
  void applyText();
  void applyState();
  void rebuildCarousel();

  void buildPages();
  void ensurePage(const QString &childId);
  void rebuildDots();
  void updatePageChrome();

  void goToPage(int index);
  void goPrev();
  void goNext();

  void onDotClicked(int index);

  ChatNodeWidget *buildChildWidget(const ChatNode &child);

  QString m_id;
  ChatNode m_node;

  ChatTree *m_tree = nullptr;

  bool m_structureBuilt = false;

  QLabel *m_glyph = nullptr;
  QLabel *m_title = nullptr;
  QLabel *m_detail = nullptr;
  QPushButton *m_abort = nullptr;

  MarkdownView *m_markdownBody = nullptr;
  QLabel *m_errorBody = nullptr;

  QWidget *m_bodyHost = nullptr;
  QVBoxLayout *m_bodyLayout = nullptr;
  QVBoxLayout *m_rootLayout = nullptr;

  QWidget *m_carouselHost = nullptr;
  QVBoxLayout *m_carouselLayout = nullptr;

  QStackedWidget *m_pages = nullptr;
  QWidget *m_chromeRow = nullptr;
  QHBoxLayout *m_chromeLayout = nullptr;

  QToolButton *m_prevButton = nullptr;
  QToolButton *m_nextButton = nullptr;
  QWidget *m_dotsHost = nullptr;
  QHBoxLayout *m_dotsLayout = nullptr;
  QLabel *m_counter = nullptr;

  QHash<QString, ChatNodeWidget *> m_pagesByChildId;
  QHash<QString, QToolButton *> m_dotsByChildId;

  int m_currentPage = -1;
  int m_lastKnownChildCount = 0;
  bool m_followTail = true;
};