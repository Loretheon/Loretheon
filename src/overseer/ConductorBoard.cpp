#include "../../include/overseer/ConductorBoard.h"

#include "../../include/overseer/ConductorQueue.h"
#include "../../include/overseer/ConductorRoster.h"
#include "../../include/overseer/DependencyGraph.h"

#include "DiagramDocument.h"
#include "DiagramToolbar.h"
#include "DiagramView.h"
#include "GraphvizRenderer.h"

#include "ThemeRegistry.h"
#include "ThemeTokens.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

struct ColumnSpec {
  QString key;
  QString title;
};

const QVector<ColumnSpec> &columnSpecs() {
  static const QVector<ColumnSpec> specs = {
      {QStringLiteral("inbox"), QStringLiteral("Inbox")},
      {QStringLiteral("routing"), QStringLiteral("Routing")},
      {QStringLiteral("delegated"), QStringLiteral("Delegated")},
      {QStringLiteral("awaiting"), QStringLiteral("Awaiting")},
      {QStringLiteral("done"), QStringLiteral("Done")},
      {QStringLiteral("failed"), QStringLiteral("Failed")},
      {QStringLiteral("rejected"), QStringLiteral("Rejected")},
  };
  return specs;
}

QString dotEscape(const QString &in) {
  QString out = in;
  out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
  out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
  out.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
  out.replace(QLatin1Char('\r'), QString());
  return out;
}

QString shortId(const QString &id) {
  return id.left(8);
}

constexpr auto SplitterStateFilename = "conductor_splitter.json";

} // namespace

ConductorBoard::ConductorBoard(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("conductorBoard"));

  m_stack = new QStackedWidget(this);

  m_conductorBoard = buildConductorBoard();
  m_stack->addWidget(m_conductorBoard);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->addWidget(m_stack);
}

void ConductorBoard::setQueue(ConductorQueue *queue) {
  if (m_queue == queue)
    return;

  if (m_queue)
    disconnect(m_queue, nullptr, this, nullptr);

  m_queue = queue;

  if (m_queue) {
    connect(m_queue, &ConductorQueue::requestAdded, this,
            [this](const QString &) { rebuild(); });
    connect(m_queue, &ConductorQueue::requestChanged, this,
            [this](const QString &) { rebuild(); });
    connect(m_queue, &ConductorQueue::requestRemoved, this,
            [this](const QString &) { rebuild(); });
  }

  rebuild();
}

void ConductorBoard::setRoster(ConductorRoster *roster) {
  if (m_roster == roster)
    return;

  if (m_roster)
    disconnect(m_roster, nullptr, this, nullptr);

  m_roster = roster;

  if (m_roster) {
    connect(m_roster, &ConductorRoster::changed, this,
            [this]() { rebuildRosterView(); });
  }

  rebuildRosterView();
}

void ConductorBoard::setDependencies(DependencyGraph *graph) {
  m_dependencies = graph;
  rebuildGraphView();
  renderDependencyGraph();
}

void ConductorBoard::setSessionFolder(const QString &folder) {
  if (m_sessionFolder == folder)
    return;

  m_sessionFolder = folder;
  loadSplitterState();
}

QString ConductorBoard::splitterStatePath() const {
  if (m_sessionFolder.isEmpty())
    return {};

  return QDir(m_sessionFolder)
      .filePath(QString::fromLatin1(SplitterStateFilename));
}

QWidget *ConductorBoard::buildConductorBoard() {
  auto *page = new QWidget(this);

  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  m_graphLabel = new QLabel(tr("(no dependencies)"), page);
  m_graphLabel->setObjectName(QStringLiteral("conductorGraphLabel"));
  m_graphLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_graphLabel->setWordWrap(true);
  layout->addWidget(m_graphLabel);

  m_rosterLabel = new QLabel(tr("(no workers)"), page);
  m_rosterLabel->setObjectName(QStringLiteral("conductorRosterLabel"));
  m_rosterLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_rosterLabel->setWordWrap(true);
  layout->addWidget(m_rosterLabel);

  // Vertical splitter: dependency graph on top, kanban below. The
  // user can drag the handle to give either side more room.
  m_boardSplitter = new QSplitter(Qt::Vertical, page);
  m_boardSplitter->setChildrenCollapsible(false);
  m_boardSplitter->setHandleWidth(6);

  m_graphPanel = buildGraphPanel();
  m_graphPanel->setMinimumHeight(80);
  m_boardSplitter->addWidget(m_graphPanel);

  auto *scroll = new QScrollArea(page);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto *host = new QWidget(scroll);

  auto *row = new QHBoxLayout(host);
  row->setContentsMargins(8, 8, 8, 8);
  row->setSpacing(8);

  for (const ColumnSpec &spec : columnSpecs()) {
    auto *column = new QWidget(host);
    column->setObjectName(QStringLiteral("conductorColumn"));
    column->setProperty("columnKey", spec.key);
    column->setMinimumWidth(220);

    auto *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(8, 8, 8, 8);
    columnLayout->setSpacing(6);

    auto *header = new QLabel(spec.title, column);
    QFont bold = header->font();
    bold.setBold(true);
    header->setFont(bold);
    columnLayout->addWidget(header);

    columnLayout->addStretch(1);

    row->addWidget(column);
  }

  scroll->setWidget(host);

  m_boardSplitter->addWidget(scroll);

  m_boardSplitter->setStretchFactor(0, 2);
  m_boardSplitter->setStretchFactor(1, 3);
  m_boardSplitter->setSizes({240, 360});

  layout->addWidget(m_boardSplitter, 1);

  return page;
}

QWidget *ConductorBoard::buildGraphPanel() {
  auto *panel = new QWidget(this);
  panel->setObjectName(QStringLiteral("conductorGraphPanel"));

  auto *layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_graphToolbar = new DiagramToolbar(panel);
  layout->addWidget(m_graphToolbar);

  m_graphView = new DiagramView(panel);
  m_graphView->setMinimumHeight(80);
  layout->addWidget(m_graphView, 1);

  m_graphDocument = new DiagramDocument(this);
  m_graphView->setDocument(m_graphDocument);

  m_graphDocument->setProjectRoot(QString());

  m_graphRenderer = new GraphvizRenderer(this);

  const ThemeTokens tokens =
      ThemeRegistry::instance().tokens(ThemeRegistry::instance().activeTheme());
  m_graphRenderer->setThemeTokens(tokens);

  connect(m_graphRenderer, &GraphvizRenderer::svgReady, this,
          [this](const QString &svg) {
            if (svg.isEmpty()) {
              m_graphDocument->clear();
              m_graphToolbar->setActionsEnabled(false);
              return;
            }
            m_graphDocument->setSvg(svg);
            m_graphToolbar->setActionsEnabled(true);
          });

  connect(m_graphRenderer, &GraphvizRenderer::renderFailed, this,
          [this](const QString &) {
            m_graphDocument->clear();
            m_graphToolbar->setActionsEnabled(false);
          });

  connect(m_graphView, &DiagramView::elementClicked, this,
          [this](const QString &id, const QString &, const QPoint &) {
            if (id.isEmpty())
              return;

            showDetail(id);
          });

  connect(m_graphToolbar, &DiagramToolbar::zoomInRequested, m_graphView,
          &DiagramView::zoomIn);
  connect(m_graphToolbar, &DiagramToolbar::zoomOutRequested, m_graphView,
          &DiagramView::zoomOut);
  connect(m_graphToolbar, &DiagramToolbar::zoomResetRequested, m_graphView,
          &DiagramView::zoomReset);
  connect(m_graphToolbar, &DiagramToolbar::fitRequested, m_graphView,
          &DiagramView::zoomFit);
  connect(m_graphToolbar, &DiagramToolbar::zoomToRequested, m_graphView,
          &DiagramView::setZoom);
  connect(m_graphView, &DiagramView::zoomChanged, m_graphToolbar,
          &DiagramToolbar::setZoom);

  m_graphToolbar->setActionsEnabled(false);
  m_graphToolbar->setZoom(1.0);

  return panel;
}

void ConductorBoard::loadSplitterState() {
  if (!m_boardSplitter)
    return;

  const QString path = splitterStatePath();

  if (path.isEmpty() || !QFileInfo::exists(path))
    return;

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());

  if (!doc.isObject())
    return;

  const QJsonArray arr = doc.object().value(QStringLiteral("sizes")).toArray();

  QList<int> sizes;

  for (const QJsonValue &v : arr)
    sizes.append(v.toInt());

  if (sizes.size() == m_boardSplitter->count())
    m_boardSplitter->setSizes(sizes);
}

void ConductorBoard::saveSplitterState() const {
  if (!m_boardSplitter)
    return;

  const QString path = splitterStatePath();

  if (path.isEmpty())
    return;

  QJsonArray arr;

  for (int size : m_boardSplitter->sizes())
    arr.append(size);

  QJsonObject obj;
  obj.insert(QStringLiteral("sizes"), arr);

  QFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void ConductorBoard::rebuild() {
  if (!m_queue || !m_conductorBoard)
    return;

  auto *scroll = m_conductorBoard->findChild<QScrollArea *>();

  if (!scroll)
    return;

  auto *host = scroll->widget();

  if (!host)
    return;

  auto *row = qobject_cast<QHBoxLayout *>(host->layout());

  if (!row)
    return;

  for (int i = 0; i < row->count(); ++i) {
    auto *column = qobject_cast<QWidget *>(row->itemAt(i)->widget());

    if (!column)
      continue;

    auto *columnLayout = qobject_cast<QVBoxLayout *>(column->layout());

    if (!columnLayout)
      continue;

    while (columnLayout->count() > 2) {
      QLayoutItem *item = columnLayout->takeAt(1);

      if (!item)
        break;

      if (QWidget *w = item->widget())
        w->deleteLater();

      delete item;
    }
  }

  const ThemeTokens tokens =
      ThemeRegistry::instance().tokens(ThemeRegistry::instance().activeTheme());

  for (const ConductorRequest &req : m_queue->all()) {
    for (int i = 0; i < row->count(); ++i) {
      auto *column = qobject_cast<QWidget *>(row->itemAt(i)->widget());

      if (!column)
        continue;

      if (column->property("columnKey").toString() != req.state)
        continue;

      auto *columnLayout = qobject_cast<QVBoxLayout *>(column->layout());

      if (!columnLayout)
        continue;

      auto *card = new QWidget(column);
      card->setObjectName(QStringLiteral("conductorCard"));
      card->setStyleSheet(
          QStringLiteral("QWidget#conductorCard { background: %1; "
                         "border: 1px solid %2; border-radius: 6px; "
                         "padding: 6px; }")
              .arg(tokens.surface0.name(), tokens.border.name()));

      auto *cardLayout = new QVBoxLayout(card);
      cardLayout->setContentsMargins(8, 6, 8, 6);
      cardLayout->setSpacing(4);

      auto *text = new QLabel(req.text, card);
      text->setWordWrap(true);
      text->setTextInteractionFlags(Qt::TextSelectableByMouse);
      cardLayout->addWidget(text);

      if (req.state == QStringLiteral("done") && !req.answer.isEmpty()) {
        auto *answer = new QLabel(req.answer, card);
        answer->setWordWrap(true);
        answer->setStyleSheet(
            QStringLiteral("color: %1;").arg(tokens.textMuted.name()));
        cardLayout->addWidget(answer);
      }

      if (req.state == QStringLiteral("failed") && !req.rejectReason.isEmpty()) {
        auto *reason = new QLabel(req.rejectReason, card);
        reason->setWordWrap(true);
        reason->setStyleSheet(
            QStringLiteral("color: %1;").arg(tokens.error.name()));
        cardLayout->addWidget(reason);
      }

      if (req.state == QStringLiteral("rejected") &&
          !req.rejectReason.isEmpty()) {
        auto *reason = new QLabel(req.rejectReason, card);
        reason->setWordWrap(true);
        reason->setStyleSheet(
            QStringLiteral("color: %1;").arg(tokens.textMuted.name()));
        cardLayout->addWidget(reason);
      }

      auto *actions = new QHBoxLayout;
      actions->setContentsMargins(0, 0, 0, 0);

      auto *detail = new QPushButton(tr("Open"), card);
      connect(detail, &QPushButton::clicked, this,
              [this, requestId = req.id]() { showDetail(requestId); });
      actions->addWidget(detail);

      const bool cancellable =
          req.state != QStringLiteral("done") &&
          req.state != QStringLiteral("failed") &&
          req.state != QStringLiteral("rejected");

      if (cancellable) {
        auto *cancel = new QPushButton(tr("Reject"), card);

        connect(cancel, &QPushButton::clicked, this,
                [this, requestId = req.id]() {
                  emit cancelRequested(requestId);
                });

        actions->addWidget(cancel);
      }

      if (req.state == QStringLiteral("failed")) {
        auto *remove = new QPushButton(tr("Remove"), card);

        connect(remove, &QPushButton::clicked, this,
                [this, requestId = req.id]() {
                  emit removeRequested(requestId);
                });

        actions->addWidget(remove);
      }

      actions->addStretch(1);

      cardLayout->addLayout(actions);

      columnLayout->insertWidget(columnLayout->count() - 1, card);

      break;
    }
  }

  rebuildGraphView();
  rebuildRosterView();
  renderDependencyGraph();
}

void ConductorBoard::rebuildGraphView() {
  if (!m_graphLabel)
    return;

  if (!m_dependencies) {
    m_graphLabel->setText(tr("(no dependency graph)"));
    return;
  }

  const QStringList nodes = m_dependencies->nodes();

  if (nodes.isEmpty()) {
    m_graphLabel->setText(tr("(no dependencies)"));
    return;
  }

  QString out = tr("Dependencies: ");

  int shown = 0;

  for (const QString &node : nodes) {
    const QStringList deps = m_dependencies->edgesTo(node);

    if (deps.isEmpty())
      continue;

    if (shown > 0)
      out += QStringLiteral("; ");

    out += QStringLiteral("%1 ← %2").arg(node, deps.join(QStringLiteral(", ")));

    ++shown;

    if (shown >= 5) {
      out += QStringLiteral(" …");
      break;
    }
  }

  if (shown == 0)
    out = tr("(no edges)");

  m_graphLabel->setText(out);
}

void ConductorBoard::rebuildRosterView() {
  if (!m_rosterLabel)
    return;

  if (!m_roster) {
    m_rosterLabel->setText(tr("(no roster)"));
    return;
  }

  const QVector<ConductorWorker> workers = m_roster->all();

  if (workers.isEmpty()) {
    m_rosterLabel->setText(tr("(no workers)"));
    return;
  }

  QString out = tr("Workers: ");

  QStringList parts;

  for (const ConductorWorker &w : workers) {
    if (w.type == QStringLiteral("file")) {
      parts.append(QStringLiteral("%1 (%2, q%3)")
                       .arg(w.id, w.domain)
                       .arg(w.queueDepth));
    } else {
      parts.append(QStringLiteral("%1 (%2)")
                       .arg(w.id, QFileInfo(w.file).fileName()));
    }
  }

  out += parts.join(QStringLiteral(", "));

  m_rosterLabel->setText(out);
}

QString ConductorBoard::nodeLabelFor(const QString &requestId) const {
  if (m_queue) {
    const ConductorRequest req = m_queue->byId(requestId);

    if (!req.id.isEmpty() && !req.text.isEmpty()) {
      QString label = req.text.simplified();

      if (label.size() > 48)
        label = label.left(45) + QStringLiteral("…");

      return label;
    }
  }

  return shortId(requestId);
}

QString ConductorBoard::buildDependencyDot() const {
  if (!m_dependencies)
    return {};

  const QStringList nodes = m_dependencies->nodes();

  if (nodes.isEmpty())
    return {};

  QString dot;

  dot += QStringLiteral("digraph dependencies {\n");
  dot += QStringLiteral("  rankdir=TB;\n");
  dot += QStringLiteral("  node [shape=box, style=rounded, fontsize=10];\n");
  dot += QStringLiteral("  edge [fontsize=9];\n");

  QSet<QString> declared;

  auto declare = [&](const QString &id) {
    if (declared.contains(id))
      return;

    declared.insert(id);

    dot += QStringLiteral("  \"%1\" [label=\"%2\\n(%3)\"];\n")
               .arg(id, dotEscape(nodeLabelFor(id)), shortId(id));
  };

  for (const QString &node : nodes) {
    declare(node);

    const QStringList deps = m_dependencies->edgesTo(node);

    for (const QString &dep : deps) {
      declare(dep);

      dot += QStringLiteral("  \"%1\" -> \"%2\";\n").arg(dep, node);
    }
  }

  dot += QStringLiteral("}\n");

  return dot;
}

void ConductorBoard::renderDependencyGraph() {
  if (!m_graphRenderer)
    return;

  const ThemeTokens tokens =
      ThemeRegistry::instance().tokens(ThemeRegistry::instance().activeTheme());
  m_graphRenderer->setThemeTokens(tokens);

  const QString dot = buildDependencyDot();

  if (dot.isEmpty()) {
    if (m_graphDocument)
      m_graphDocument->clear();

    if (m_graphToolbar)
      m_graphToolbar->setActionsEnabled(false);

    return;
  }

  m_graphRenderer->renderToSvgAsync(dot);
}

void ConductorBoard::showDetail(const QString &requestId) {
  if (requestId.isEmpty())
    return;

  m_detailRequestId = requestId;

  auto *panel = buildDetailPanel(requestId);

  if (!panel)
    return;

  m_stack->addWidget(panel);
  m_stack->setCurrentWidget(panel);
}

void ConductorBoard::popDetail() {
  if (m_stack->count() <= 1)
    return;

  QWidget *current = m_stack->currentWidget();

  if (current == m_conductorBoard)
    return;

  m_stack->removeWidget(current);
  current->deleteLater();

  m_stack->setCurrentWidget(m_conductorBoard);

  m_detailRequestId.clear();
}

QWidget *ConductorBoard::buildDetailPanel(const QString &requestId) {
  if (!m_queue)
    return nullptr;

  const ConductorRequest req = m_queue->byId(requestId);

  if (req.id.isEmpty())
    return nullptr;

  auto *panel = new QWidget(this);

  auto *layout = new QVBoxLayout(panel);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);

  auto *back = new QPushButton(tr("← Back to board"), panel);

  connect(back, &QPushButton::clicked, this, &ConductorBoard::popDetail);

  layout->addWidget(back);

  auto *header = new QLabel(req.text, panel);
  header->setWordWrap(true);
  QFont bold = header->font();
  bold.setBold(true);
  bold.setPointSize(bold.pointSize() + 1);
  header->setFont(bold);
  layout->addWidget(header);

  auto *stateLabel = new QLabel(tr("State: %1").arg(req.state), panel);
  layout->addWidget(stateLabel);

  if (!req.workerId.isEmpty()) {
    auto *worker = new QLabel(tr("Worker: %1").arg(req.workerId), panel);
    layout->addWidget(worker);
  }

  if (!req.answer.isEmpty()) {
    auto *answer = new QLabel(req.answer, panel);
    answer->setWordWrap(true);
    layout->addWidget(answer);
  }

  if (!req.rejectReason.isEmpty()) {
    auto *reason = new QLabel(req.rejectReason, panel);
    reason->setWordWrap(true);
    layout->addWidget(reason);
  }

  layout->addStretch(1);

  return panel;
}

void ConductorBoard::changeEvent(QEvent *event) {
  if (event && (event->type() == QEvent::PaletteChange ||
                event->type() == QEvent::StyleChange)) {
    rebuild();
  }

  QWidget::changeEvent(event);
}