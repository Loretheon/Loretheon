#pragma once

#include "ChatNode.h"

#include <QHash>
#include <QWidget>

class ChatTree;
class MarkdownView;

class QLabel;
class QPushButton;
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

  void openChild(int index);
  void closeChild(const QString &childId);
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

  QWidget *m_arrowColumn = nullptr;
  QVBoxLayout *m_arrowLayout = nullptr;
  QWidget *m_carouselHost = nullptr;
  QVBoxLayout *m_carouselLayout = nullptr;

  QHash<QString, ChatNodeWidget *> m_openChildren;
};