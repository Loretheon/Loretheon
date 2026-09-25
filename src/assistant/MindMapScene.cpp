#include "../../include/assistant/MindMapScene.h"

#include "../../include/assistant/AssistantMemory.h"
#include "../../include/assistant/AssistantProfile.h"
#include "../../include/assistant/MindMapNode.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace {

constexpr qreal kHorizontalSpacing = 90.0;
constexpr qreal kVerticalSpacing = 24.0;

QString readFile(const QString &path) {
  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  return stream.readAll();
}

QString firstHeading(const QString &body, const QString &fallback) {
  const QStringList lines = body.split(QChar('\n'));

  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();

    if (trimmed.startsWith(QChar('#'))) {
      QString heading = trimmed;

      while (heading.startsWith(QChar('#'))) {
        heading.remove(0, 1);
      }

      heading = heading.trimmed();

      if (!heading.isEmpty()) {
        return heading;
      }
    }
  }

  return fallback;
}

QString firstParagraph(const QString &body, int maxLength) {
  const QStringList lines = body.split(QChar('\n'));

  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();

    if (trimmed.isEmpty() || trimmed.startsWith(QChar('#'))) {
      continue;
    }

    QString flat = trimmed;
    flat.replace(QRegularExpression(QStringLiteral("\\s+")),
                 QStringLiteral(" "));

    if (flat.length() > maxLength) {
      flat = flat.left(maxLength - 1) + QStringLiteral("…");
    }

    return flat;
  }

  return {};
}

QVector<QPair<QString, QString>> factsFromBody(const QString &body) {
  QVector<QPair<QString, QString>> facts;

  const QStringList lines = body.split(QChar('\n'));

  QString pendingTime;

  for (int i = 0; i < lines.size(); ++i) {
    const QString trimmed = lines.at(i).trimmed();

    if (trimmed.startsWith(QStringLiteral("## "))) {
      pendingTime = trimmed.mid(3).trimmed();
      continue;
    }

    if (!pendingTime.isEmpty() && !trimmed.isEmpty()) {
      QString fact = trimmed;
      fact.replace(QRegularExpression(QStringLiteral("\\s+")),
                   QStringLiteral(" "));

      if (fact.length() > 100) {
        fact = fact.left(99) + QStringLiteral("…");
      }

      facts.append({pendingTime, fact});
      pendingTime.clear();
    }
  }

  return facts;
}

} // namespace

MindMapScene::MindMapScene(QObject *parent) : QGraphicsScene(parent) {}

void MindMapScene::build(const QString &assistantRoot) {
  clear();
  m_nodesByPath.clear();

  if (assistantRoot.isEmpty()) {
    emit contentChanged();
    return;
  }

  const QString identityPath =
      QDir(assistantRoot).filePath(QStringLiteral("identity.md"));
  const QString userPath =
      QDir(assistantRoot).filePath(QStringLiteral("user.md"));
  const QString selfPath =
      QDir(assistantRoot).filePath(QStringLiteral("self.md"));
  const QString memoriesPath =
      QDir(assistantRoot).filePath(QStringLiteral("memories"));

  m_root = new MindMapNode(MindMapNode::Kind::Root,
                           QStringLiteral("Lore"));

  addToScene(m_root);

  MindMapNode *identity =
      addProfileFile(QStringLiteral("Identity"), identityPath,
                     readFile(identityPath));
  MindMapNode *user =
      addProfileFile(QStringLiteral("User"), userPath,
                     readFile(userPath));
  MindMapNode *self =
      addProfileFile(QStringLiteral("Self"), selfPath,
                     readFile(selfPath));

  m_root->addChild(identity);
  m_root->addChild(user);
  m_root->addChild(self);

  MindMapNode *memories =
      new MindMapNode(MindMapNode::Kind::File,
                      QStringLiteral("Memories"));
  addToScene(memories);
  m_root->addChild(memories);

  QDir memoriesDir(memoriesPath);

  if (memoriesDir.exists()) {
    const QString topicsRoot =
        memoriesDir.filePath(QStringLiteral("topics"));

    QDir topicsDir(topicsRoot);

    if (topicsDir.exists()) {
      const QStringList topicFiles = topicsDir.entryList(
          QStringList{QStringLiteral("*.md")}, QDir::Files, QDir::Name);

      for (const QString &fileName : topicFiles) {
        const QString path = topicsDir.filePath(fileName);

        MindMapNode *topic =
            addMemoryFile(path);

        if (topic) {
          memories->addChild(topic);
        }
      }
    }

    const QString sessionsRoot =
        memoriesDir.filePath(QStringLiteral("sessions"));

    QDir sessionsDir(sessionsRoot);

    if (sessionsDir.exists()) {
      const QStringList sessionFiles = sessionsDir.entryList(
          QStringList{QStringLiteral("*.md")}, QDir::Files, QDir::Name);

      for (const QString &fileName : sessionFiles) {
        const QString path = sessionsDir.filePath(fileName);

        MindMapNode *session = addMemoryFile(path);

        if (session) {
          memories->addChild(session);
        }
      }
    }
  }

  layoutTree();

  emit contentChanged();
}

MindMapNode *MindMapScene::addProfileFile(const QString &title,
                                          const QString &path,
                                          const QString &body) {
  const QString detail = firstParagraph(body, 80);

  auto *node = new MindMapNode(MindMapNode::Kind::File, title, detail);

  addToScene(node);

  m_nodesByPath.insert(path, node);

  return node;
}

MindMapNode *MindMapScene::addMemoryFile(const QString &path) {
  const QString body = readFile(path);

  const QString title =
      firstHeading(body, QFileInfo(path).completeBaseName());

  auto *node = new MindMapNode(MindMapNode::Kind::Topic, title);

  addToScene(node);

  m_nodesByPath.insert(path, node);

  const QVector<QPair<QString, QString>> facts = factsFromBody(body);

  for (const auto &fact : facts) {
    auto *factNode = new MindMapNode(MindMapNode::Kind::Fact,
                                     fact.second, fact.first);

    addToScene(factNode);

    node->addChild(factNode);
  }

  return node;
}

void MindMapScene::addToScene(MindMapNode *node) {
  if (node) {
    addItem(node);
  }
}

MindMapNode *MindMapScene::nodeFor(const QString &path) const {
  return m_nodesByPath.value(path, nullptr);
}

void MindMapScene::focusOn(MindMapNode *node) {
  if (!node) {
    clearFocus();
    return;
  }

  QSet<MindMapNode *> visible;

  std::function<void(MindMapNode *)> collect = [&](MindMapNode *n) {
    if (!n) {
      return;
    }

    visible.insert(n);

    for (MindMapNode *child : n->children()) {
      collect(child);
    }
  };

  collect(node);

  // Also keep the ancestors visible so the path from the root is clear.
  MindMapNode *parent = qgraphicsitem_cast<MindMapNode *>(node->parentItem());

  while (parent) {
    visible.insert(parent);
    parent = qgraphicsitem_cast<MindMapNode *>(parent->parentItem());
  }

  std::function<void(MindMapNode *)> apply =
      [&](MindMapNode *n) {
        if (!n) {
          return;
        }

        const bool isVisible = visible.contains(n);

        n->setFocused(n == node);
        n->setHighlighted(isVisible && n != node);

        if (n != node) {
          n->setOpacity(isVisible ? 1.0 : 0.35);
        } else {
          n->setOpacity(1.0);
        }

        for (MindMapNode *child : n->children()) {
          apply(child);
        }
      };

  apply(m_root);
}

void MindMapScene::clearFocus() {
  std::function<void(MindMapNode *)> apply =
      [&](MindMapNode *n) {
        if (!n) {
          return;
        }

        n->setFocused(false);
        n->setHighlighted(false);
        n->setOpacity(1.0);

        for (MindMapNode *child : n->children()) {
          apply(child);
        }
      };

  apply(m_root);
}

void MindMapScene::setVisibleDepth(int depth) {
  m_visibleDepth = qBound(1, depth, 8);

  std::function<void(MindMapNode *, int)> apply =
      [&](MindMapNode *n, int level) {
        if (!n) {
          return;
        }

        n->setVisible(level <= m_visibleDepth);

        for (MindMapNode *child : n->children()) {
          apply(child, level + 1);
        }
      };

  apply(m_root, 0);

  layoutTree();
}

void MindMapScene::layoutTree() {
  if (!m_root) {
    return;
  }

  // Compute the height each subtree needs, then place nodes with a
  // simple top-down pass.

  std::function<qreal(MindMapNode *)> measure =
      [&](MindMapNode *node) -> qreal {
    if (!node) {
      return 0.0;
    }

    const qreal ownHeight = node->boundingRect().height();

    qreal childrenHeight = 0.0;
    int visibleChildren = 0;

    for (MindMapNode *child : node->children()) {
      if (!child || !child->isVisible()) {
        continue;
      }

      childrenHeight += measure(child);
      ++visibleChildren;
    }

    if (visibleChildren > 1) {
      childrenHeight += kVerticalSpacing * (visibleChildren - 1);
    }

    return qMax(ownHeight, childrenHeight);
  };

  std::function<void(MindMapNode *, qreal, qreal)> place =
      [&](MindMapNode *node, qreal x, qreal centerY) {
        if (!node || !node->isVisible()) {
          return;
        }

        const QRectF rect = node->boundingRect();
        node->setPos(x + rect.width() / 2.0, centerY);

        QList<MindMapNode *> visibleChildren;

        for (MindMapNode *child : node->children()) {
          if (child && child->isVisible()) {
            visibleChildren.append(child);
          }
        }

        if (visibleChildren.isEmpty()) {
          return;
        }

        qreal totalHeight = 0.0;

        for (MindMapNode *child : visibleChildren) {
          totalHeight += measure(child);
        }

        totalHeight += kVerticalSpacing * (visibleChildren.size() - 1);

        qreal cursorY = centerY - totalHeight / 2.0;

        for (MindMapNode *child : visibleChildren) {
          const qreal subtreeHeight = measure(child);

          const qreal childCenterY = cursorY + subtreeHeight / 2.0;

          place(child, x + rect.width() + kHorizontalSpacing,
                childCenterY);

          cursorY += subtreeHeight + kVerticalSpacing;
        }
      };

  const qreal rootHeight = measure(m_root);

  place(m_root, 0.0, rootHeight / 2.0);

  setSceneRect(itemsBoundingRect().adjusted(-40, -40, 40, 40));
}