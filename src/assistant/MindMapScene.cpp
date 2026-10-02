#include "../../include/assistant/MindMapScene.h"

#include "../../include/app/theme/ThemeRegistry.h"
#include "../../include/assistant/MindMapNode.h"
#include "../../include/overseer/OverseerSessionManager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsSceneMouseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>

#include <cmath>
#include <functional>

namespace {

constexpr qreal kRepulsion = 220000.0;
constexpr qreal kSpringLength = 220.0;
constexpr qreal kSpringStrength = 0.045;
constexpr qreal kCenterPull = 0.006;
constexpr qreal kDamping = 0.72;
constexpr qreal kMaxDisplacement = 60.0;
constexpr qreal kSettleEpsilon = 0.4;

constexpr int kFullSimTicks = 320;
constexpr int kIncrementalTicks = 120;
constexpr int kSettleChecks = 12;

// The scene rect is a very large fixed rectangle, so the pan area is
// effectively unbounded and does not shrink or grow as nodes move.
constexpr qreal kInfiniteExtent = 100000.0;

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

QString factCacheId(const QString &topicPath, const QString &factText) {
  const QByteArray hash = QCryptographicHash::hash(
      (topicPath + QChar('|') + factText).toUtf8(),
      QCryptographicHash::Sha1);

  return QStringLiteral("fact:") +
         QString::fromLatin1(hash.toHex().left(16));
}

QString fileCacheId(const QString &absolutePath) {
  return QStringLiteral("file:") + absolutePath;
}

QString sessionCacheId(const QString &name) {
  return QStringLiteral("session:") + name;
}

QString hubCacheId(const QString &name) {
  return QStringLiteral("hub:") + name;
}

} // namespace

MindMapScene::MindMapScene(QObject *parent) : QGraphicsScene(parent) {
  setBackgroundBrush(Qt::NoBrush);

  setSceneRect(-kInfiniteExtent, -kInfiniteExtent,
               kInfiniteExtent * 2.0, kInfiniteExtent * 2.0);

  m_simTimer = new QTimer(this);
  m_simTimer->setInterval(16);
  connect(m_simTimer, &QTimer::timeout, this,
          &MindMapScene::onSimulationTick);
}

MindMapScene::~MindMapScene() = default;

void MindMapScene::setOverseerManager(OverseerSessionManager *manager) {
  m_overseer = manager;
}

void MindMapScene::setAssistantRoot(const QString &root) {
  m_assistantRoot = root;
}

QString MindMapScene::cachePath() const {
  if (m_assistantRoot.isEmpty()) {
    return {};
  }

  return QDir(m_assistantRoot)
      .filePath(QStringLiteral("memories/.mindmap.json"));
}

void MindMapScene::build() {
  clear();
  m_nodesById.clear();
  m_nodesByPath.clear();
  m_allNodes.clear();
  m_edges.clear();
  m_root = nullptr;

  if (m_assistantRoot.isEmpty()) {
    emit contentChanged();
    return;
  }

  m_root = new MindMapNode(MindMapNode::Kind::Root,
                           QStringLiteral("Lore"));
  m_root->setCacheId(QStringLiteral("root"));
  addToScene(m_root);

  auto *profileHub = new MindMapNode(MindMapNode::Kind::Hub,
                                     QStringLiteral("Profile"));
  profileHub->setCacheId(hubCacheId(QStringLiteral("Profile")));
  addToScene(profileHub);
  m_root->addChild(profileHub);

  const QString identityPath =
      QDir(m_assistantRoot).filePath(QStringLiteral("identity.md"));
  const QString userPath =
      QDir(m_assistantRoot).filePath(QStringLiteral("user.md"));
  const QString selfPath =
      QDir(m_assistantRoot).filePath(QStringLiteral("self.md"));

  profileHub->addChild(addProfileFile(QStringLiteral("Identity"),
                                      identityPath,
                                      readFile(identityPath)));
  profileHub->addChild(addProfileFile(QStringLiteral("User"), userPath,
                                      readFile(userPath)));
  profileHub->addChild(addProfileFile(QStringLiteral("Self"), selfPath,
                                      readFile(selfPath)));

  auto *memoryHub = new MindMapNode(MindMapNode::Kind::Hub,
                                    QStringLiteral("Memory"));
  memoryHub->setCacheId(hubCacheId(QStringLiteral("Memory")));
  addToScene(memoryHub);
  m_root->addChild(memoryHub);

  const QString memoriesPath =
      QDir(m_assistantRoot).filePath(QStringLiteral("memories"));

  QDir memoriesDir(memoriesPath);

  if (memoriesDir.exists()) {
    QDir topicsDir(memoriesDir.filePath(QStringLiteral("topics")));

    if (topicsDir.exists()) {
      const QStringList files = topicsDir.entryList(
          QStringList{QStringLiteral("*.md")}, QDir::Files, QDir::Name);

      for (const QString &fileName : files) {
        MindMapNode *topic = addMemoryTopic(topicsDir.filePath(fileName));

        if (topic) {
          memoryHub->addChild(topic);
        }
      }
    }

    QDir sessionsDir(memoriesDir.filePath(QStringLiteral("sessions")));

    if (sessionsDir.exists()) {
      const QStringList files = sessionsDir.entryList(
          QStringList{QStringLiteral("*.md")}, QDir::Files, QDir::Name);

      for (const QString &fileName : files) {
        MindMapNode *session =
            addMemorySession(sessionsDir.filePath(fileName));

        if (session) {
          memoryHub->addChild(session);
        }
      }
    }
  }

  auto *overseerHub = new MindMapNode(MindMapNode::Kind::Hub,
                                      QStringLiteral("Overseer"));
  overseerHub->setCacheId(hubCacheId(QStringLiteral("Overseer")));
  addToScene(overseerHub);
  m_root->addChild(overseerHub);

  if (m_overseer) {
    const auto sessions = m_overseer->listSessionsWithDescriptions();

    for (const auto &info : sessions) {
      MindMapNode *node =
          addOverseerSession(info.name, info.description);

      if (node) {
        overseerHub->addChild(node);
      }
    }
  }

  collectAllNodes();
  collectTreeEdges();
  buildCrossLinks();

  seedPositions();

  if (hasCache()) {
    loadLayoutFromDisk();
  } else {
    startSimulation(kFullSimTicks);
  }

  emit contentChanged();
}

void MindMapScene::refresh() {
  QHash<QString, QPointF> savedPositions;

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    const QString id = node->cacheId();

    if (!id.isEmpty()) {
      savedPositions.insert(id, node->pos());
    }
  }

  build();

  int newNodes = 0;

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    const QString id = node->cacheId();

    if (id.isEmpty()) {
      continue;
    }

    const auto it = savedPositions.constFind(id);

    if (it != savedPositions.constEnd()) {
      node->setPos(it.value());
      node->setPinned(true);
    } else {
      auto *parent =
          qgraphicsitem_cast<MindMapNode *>(node->parentItem());

      if (parent) {
        node->setPos(parent->pos() +
                     QPointF(80.0, 80.0) * (newNodes % 4));
      }

      node->setPinned(false);
      ++newNodes;
    }
  }

  if (newNodes > 0) {
    startSimulation(kIncrementalTicks);
  } else {
    saveLayout();
  }

  emit contentChanged();
}

void MindMapScene::rebuildLayout() {
  if (!m_root) {
    return;
  }

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    node->setPinned(false);
  }

  seedPositions();
  startSimulation(kFullSimTicks);
}

MindMapNode *MindMapScene::addProfileFile(const QString &title,
                                          const QString &path,
                                          const QString &body) {
  const QString detail = body.trimmed();

  auto *node =
      new MindMapNode(MindMapNode::Kind::ProfileFile, title, detail);
  node->setPath(path);
  node->setCacheId(fileCacheId(path));

  addToScene(node);
  m_nodesByPath.insert(path, node);
  m_nodesById.insert(node->cacheId(), node);

  return node;
}

MindMapNode *MindMapScene::addMemoryTopic(const QString &path) {
  const QString body = readFile(path);

  const QString title =
      firstHeading(body, QFileInfo(path).completeBaseName());

  auto *node = new MindMapNode(MindMapNode::Kind::MemoryTopic, title,
                               body.trimmed());
  node->setPath(path);
  node->setCacheId(fileCacheId(path));

  addToScene(node);
  m_nodesByPath.insert(path, node);
  m_nodesById.insert(node->cacheId(), node);

  const auto facts = factsFromBody(body);

  for (const auto &fact : facts) {
    auto *factNode = new MindMapNode(MindMapNode::Kind::Fact,
                                     fact.second, fact.first);
    factNode->setPath(path);
    factNode->setCacheId(factCacheId(path, fact.second));

    addToScene(factNode);
    m_nodesById.insert(factNode->cacheId(), factNode);
    node->addChild(factNode);
  }

  return node;
}

MindMapNode *MindMapScene::addMemorySession(const QString &path) {
  const QString body = readFile(path);

  const QString title =
      firstHeading(body, QFileInfo(path).completeBaseName());

  auto *node = new MindMapNode(MindMapNode::Kind::MemorySession, title,
                               body.trimmed());
  node->setPath(path);
  node->setCacheId(fileCacheId(path));

  addToScene(node);
  m_nodesByPath.insert(path, node);
  m_nodesById.insert(node->cacheId(), node);

  return node;
}

MindMapNode *MindMapScene::addOverseerSession(const QString &name,
                                              const QString &description) {
  QString detail = description.trimmed();

  auto *node = new MindMapNode(MindMapNode::Kind::OverseerSession,
                               name, detail);
  node->setSessionName(name);
  node->setCacheId(sessionCacheId(name));

  addToScene(node);
  m_nodesById.insert(node->cacheId(), node);

  return node;
}

void MindMapScene::addToScene(MindMapNode *node) {
  if (node) {
    addItem(node);
  }
}

void MindMapScene::collectAllNodes() {
  m_allNodes.clear();

  if (!m_root) {
    return;
  }

  std::function<void(MindMapNode *)> visit = [&](MindMapNode *node) {
    if (!node) return;

    m_allNodes.append(node);

    if (!node->cacheId().isEmpty()) {
      m_nodesById.insert(node->cacheId(), node);
    }

    for (MindMapNode *child : node->children()) {
      visit(child);
    }
  };

  visit(m_root);
}

void MindMapScene::collectTreeEdges() {
  m_edges.clear();

  std::function<void(MindMapNode *)> visit = [&](MindMapNode *node) {
    if (!node) return;

    for (MindMapNode *child : node->children()) {
      m_edges.append(qMakePair(node, child));
      visit(child);
    }
  };

  visit(m_root);
}

void MindMapScene::buildCrossLinks() {
  const QRegularExpression bracketRef(
      QStringLiteral("\\[\\[([^\\]]+)\\]\\]"));

  QHash<QString, MindMapNode *> pathIndex;

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    if (!node->path().isEmpty()) {
      pathIndex.insert(node->path(), node);
    }
  }

  auto linkFromBody = [&](MindMapNode *source, const QString &body) {
    if (!source) return;

    const auto matches = bracketRef.globalMatch(body);

    auto it = matches;
    while (it.hasNext()) {
      const auto match = it.next();
      const QString ref = match.captured(1).trimmed();

      MindMapNode *target = pathIndex.value(ref, nullptr);

      if (!target) {
        for (MindMapNode *candidate : std::as_const(m_allNodes)) {
          if (!candidate->path().isEmpty() &&
              candidate->path().endsWith(ref)) {
            target = candidate;
            break;
          }
        }
      }

      if (target && target != source) {
        source->addCrossLink(target);
      }
    }
  };

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    if (node->kind() == MindMapNode::Kind::ProfileFile ||
        node->kind() == MindMapNode::Kind::MemoryTopic ||
        node->kind() == MindMapNode::Kind::MemorySession) {
      linkFromBody(node, readFile(node->path()));
    }
  }
}

void MindMapScene::seedPositions() {
  if (!m_root) {
    return;
  }

  m_root->setPos(0.0, 0.0);

  const auto hubs = m_root->children();
  const int hubCount = hubs.size();

  if (hubCount == 0) {
    return;
  }

  const qreal baseAngle = -M_PI / 2.0;
  const qreal step = (2.0 * M_PI) / hubCount;

  for (int h = 0; h < hubCount; ++h) {
    MindMapNode *hub = hubs.at(h);

    const qreal angle = baseAngle + step * h;

    const QPointF hubPos(std::cos(angle) * 420.0,
                         std::sin(angle) * 420.0);

    hub->setPos(hubPos);

    const auto leaves = hub->children();
    const int leafCount = leaves.size();

    if (leafCount == 0) {
      continue;
    }

    const qreal leafSpread = 1.5;
    const qreal leafStep =
        leafCount > 1 ? leafSpread / (leafCount - 1) : 0.0;
    const qreal leafStart = angle - leafSpread / 2.0;

    for (int l = 0; l < leafCount; ++l) {
      MindMapNode *leaf = leaves.at(l);

      const qreal leafAngle = leafStart + leafStep * l;

      const QPointF leafPos(
          hubPos + QPointF(std::cos(leafAngle) * 300.0,
                           std::sin(leafAngle) * 300.0));

      leaf->setPos(leafPos);

      const auto facts = leaf->children();
      const int factCount = facts.size();

      if (factCount == 0) {
        continue;
      }

      const qreal factSpread = 1.2;
      const qreal factStep =
          factCount > 1 ? factSpread / (factCount - 1) : 0.0;
      const qreal factStart = leafAngle - factSpread / 2.0;

      for (int f = 0; f < factCount; ++f) {
        MindMapNode *fact = facts.at(f);

        const qreal factAngle = factStart + factStep * f;

        const QPointF factPos(
            leafPos + QPointF(std::cos(factAngle) * 220.0,
                              std::sin(factAngle) * 220.0));

        fact->setPos(factPos);
      }
    }
  }
}

bool MindMapScene::hasCache() const {
  const QString path = cachePath();

  if (path.isEmpty()) {
    return false;
  }

  return QFileInfo::exists(path);
}

void MindMapScene::loadLayoutFromDisk() {
  const QString path = cachePath();

  if (path.isEmpty()) {
    return;
  }

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }

  const QByteArray bytes = file.readAll();
  file.close();

  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(bytes, &error);

  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    return;
  }

  const QJsonObject root = doc.object();

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    const QString id = node->cacheId();

    if (id.isEmpty()) {
      continue;
    }

    const QJsonValue value = root.value(id);

    if (!value.isObject()) {
      continue;
    }

    const QJsonObject entry = value.toObject();

    const qreal x = entry.value(QStringLiteral("x")).toDouble();
    const qreal y = entry.value(QStringLiteral("y")).toDouble();

    node->setPos(x, y);
    node->setPinned(true);
  }
}

void MindMapScene::saveLayout() const {
  const QString path = cachePath();

  if (path.isEmpty()) {
    return;
  }

  QJsonObject root;

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    const QString id = node->cacheId();

    if (id.isEmpty()) {
      continue;
    }

    const QPointF pos = node->pos();

    QJsonObject entry;
    entry.insert(QStringLiteral("x"), pos.x());
    entry.insert(QStringLiteral("y"), pos.y());

    root.insert(id, entry);
  }

  const QFileInfo info(path);

  QDir().mkpath(info.absolutePath());

  QFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return;
  }

  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  file.close();
}

void MindMapScene::pinNode(MindMapNode *node, const QPointF &pos) {
  if (!node) {
    return;
  }

  node->setPos(pos);
  node->setPinned(true);

  saveLayout();
}

void MindMapScene::startSimulation(int maxTicks) {
  if (m_allNodes.isEmpty()) {
    return;
  }

  m_simTicksLeft = maxTicks;
  m_simIteration = 0;
  m_lastMaxDisplacement = 0.0;
  m_settleRun = 0;

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    node->setVelocity(QPointF(0.0, 0.0));
  }

  if (!m_simTimer->isActive()) {
    m_simTimer->start();
  }
}

void MindMapScene::onSimulationTick() {
  if (m_allNodes.isEmpty()) {
    m_simTimer->stop();
    return;
  }

  if (m_simTicksLeft <= 0) {
    m_simTimer->stop();
    saveLayout();
    return;
  }

  ++m_simIteration;

  const qreal cooling =
      static_cast<qreal>(m_simTicksLeft) /
      static_cast<qreal>(kFullSimTicks + 1);

  const qreal maxStep = kMaxDisplacement * qMax(0.15, cooling);

  QHash<MindMapNode *, QPointF> forces;

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    forces.insert(node, QPointF(0.0, 0.0));
  }

  for (int i = 0; i < m_allNodes.size(); ++i) {
    MindMapNode *a = m_allNodes.at(i);

    for (int j = i + 1; j < m_allNodes.size(); ++j) {
      MindMapNode *b = m_allNodes.at(j);

      QPointF delta = a->pos() - b->pos();

      qreal dist = std::hypot(delta.x(), delta.y());

      if (dist < 1.0) {
        delta = QPointF(1.0, 0.5);
        dist = 1.0;
      }

      const qreal force = kRepulsion / (dist * dist);

      const QPointF dir = delta / dist;

      forces[a] += dir * force;
      forces[b] -= dir * force;
    }
  }

  for (const auto &edge : std::as_const(m_edges)) {
    MindMapNode *a = edge.first;
    MindMapNode *b = edge.second;

    QPointF delta = b->pos() - a->pos();

    qreal dist = std::hypot(delta.x(), delta.y());

    if (dist < 1.0) {
      dist = 1.0;
    }

    const qreal displacement = dist - kSpringLength;

    const qreal force = displacement * kSpringStrength;

    const QPointF dir = delta / dist;

    forces[a] += dir * force;
    forces[b] -= dir * force;
  }

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    forces[node] -= node->pos() * kCenterPull;
  }

  qreal maxDisplacementThisTick = 0.0;

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    if (node->isPinned()) {
      continue;
    }

    QPointF velocity = node->velocity() + forces.value(node);

    velocity *= kDamping;

    const qreal speed = std::hypot(velocity.x(), velocity.y());

    if (speed > maxStep) {
      velocity = velocity / speed * maxStep;
    }

    node->setVelocity(velocity);

    maxDisplacementThisTick =
        qMax(maxDisplacementThisTick, speed);

    if (speed < kSettleEpsilon * 0.1) {
      continue;
    }

    node->setPos(node->pos() + velocity);
  }

  m_lastMaxDisplacement = maxDisplacementThisTick;

  --m_simTicksLeft;

  if (m_lastMaxDisplacement < kSettleEpsilon) {
    ++m_settleRun;

    if (m_settleRun >= kSettleChecks) {
      m_simTimer->stop();
      m_simTicksLeft = 0;
      m_settleRun = 0;
      saveLayout();
      return;
    }
  } else {
    m_settleRun = 0;
  }
}

void MindMapScene::focusOn(MindMapNode *node) {
  if (!node) {
    clearFocus();
    return;
  }

  QSet<MindMapNode *> inPath;

  MindMapNode *cursor = node;

  while (cursor) {
    inPath.insert(cursor);

    auto *parent = qgraphicsitem_cast<MindMapNode *>(cursor->parentItem());
    cursor = parent;
  }

  QSet<MindMapNode *> inSubtree;

  std::function<void(MindMapNode *)> collect = [&](MindMapNode *n) {
    if (!n) return;
    inSubtree.insert(n);
    for (MindMapNode *c : n->children()) collect(c);
    for (MindMapNode *c : n->crossLinks()) inSubtree.insert(c);
  };
  collect(node);

  std::function<void(MindMapNode *)> apply = [&](MindMapNode *n) {
    if (!n) return;

    const bool isPath = inPath.contains(n);
    const bool isSubtree = inSubtree.contains(n);
    const bool isVisible = isPath || isSubtree;

    n->setFocused(n == node);
    n->setHighlighted(isVisible && n != node);
    n->setDimmed(!isVisible);

    for (MindMapNode *c : n->children()) apply(c);
  };

  apply(m_root);
}

void MindMapScene::clearFocus() {
  std::function<void(MindMapNode *)> apply = [&](MindMapNode *n) {
    if (!n) return;

    n->setFocused(false);
    n->setHighlighted(false);
    n->setDimmed(false);

    for (MindMapNode *c : n->children()) apply(c);
  };

  apply(m_root);
}

void MindMapScene::openNode(MindMapNode *node) {
  if (!node) {
    return;
  }

  if (node->kind() == MindMapNode::Kind::OverseerSession) {
    emit sessionOpenRequested(node->sessionName());
    return;
  }

  const QString path = node->path();

  if (!path.isEmpty()) {
    emit openRequested(path);
  }
}

void MindMapScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  QGraphicsItem *item = itemAt(event->scenePos(), QTransform());

  if (item) {
    auto *node = qgraphicsitem_cast<MindMapNode *>(item);

    if (node) {
      openNode(node);
      event->accept();
      return;
    }
  }

  QGraphicsScene::mousePressEvent(event);
}

void MindMapScene::drawForeground(QPainter *painter, const QRectF &rect) {
  Q_UNUSED(rect);

  if (!m_root) {
    return;
  }

  const ThemeTokens tokens = ThemeRegistry::instance().tokens(
      ThemeRegistry::instance().activeTheme());

  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setBrush(Qt::NoBrush);

  auto drawEdge = [&](MindMapNode *a, MindMapNode *b, bool cross) {
    if (!a || !b) {
      return;
    }

    const QPointF from = a->childAnchor(b->scenePos());
    const QPointF to = b->parentAnchor(a->scenePos());

    const QPointF mid = (from + to) * 0.5;

    const qreal dx = to.x() - from.x();
    const qreal dy = to.y() - from.y();

    const qreal length = std::hypot(dx, dy);

    const qreal bow = cross ? qMin(length * 0.05, 20.0)
                            : qMin(length * 0.18, 48.0);

    const QPointF perp(-dy / (length + 1e-9), dx / (length + 1e-9));

    const QPointF control1 =
        from + (mid - from) * 0.55 + perp * bow;
    const QPointF control2 =
        to + (mid - to) * 0.55 + perp * bow;

    QPainterPath path;
    path.moveTo(from);
    path.cubicTo(control1, control2, to);

    const bool dim = a->isDimmed() || b->isDimmed();

    QColor color = cross ? tokens.divider
                         : (b->isFocused() || a->isFocused()
                                ? tokens.accent
                                : tokens.border);

    color.setAlpha(dim ? 24
                       : (cross ? 60
                                : (b->isHighlighted() ? 190 : 110)));

    const qreal width = cross ? 1.0 : 1.4;

    painter->setPen(
        QPen(color, width, cross ? Qt::DashLine : Qt::SolidLine,
             Qt::RoundCap));

    painter->drawPath(path);
  };

  std::function<void(MindMapNode *)> walk = [&](MindMapNode *node) {
    if (!node) return;

    for (MindMapNode *child : node->children()) {
      drawEdge(node, child, false);
      walk(child);
    }
  };

  walk(m_root);

  QSet<QPair<QString, QString>> seen;

  for (MindMapNode *node : std::as_const(m_allNodes)) {
    for (MindMapNode *peer : node->crossLinks()) {
      const QString a = node->cacheId();
      const QString b = peer->cacheId();

      if (a.isEmpty() || b.isEmpty() || a == b) {
        continue;
      }

      const QPair<QString, QString> key =
          a < b ? qMakePair(a, b) : qMakePair(b, a);

      if (seen.contains(key)) {
        continue;
      }

      seen.insert(key);

      drawEdge(node, peer, true);
    }
  }
}