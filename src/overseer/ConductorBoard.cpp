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

#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QStackedWidget>
#include <QTabWidget>
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
    {QStringLiteral("skipped"), QStringLiteral("Skipped")},
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

} // namespace

ConductorBoard::ConductorBoard(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("conductorBoard"));

  m_stack = new QStackedWidget(this);

  m_conductorBoard = new QWidget(this);
  auto *boardLayout = new QVBoxLayout(m_conductorBoard);
  boardLayout->setContentsMargins(0, 0, 0, 0);
  boardLayout->setSpacing(0);

  m_tabs = new QTabWidget(m_conductorBoard);
  m_tabs->setDocumentMode(true);

  m_tabs->addTab(buildGraphTab(), tr("Graph"));
  m_tabs->addTab(buildKanbanTab(), tr("Kanban"));

  boardLayout->addWidget(m_tabs, 1);

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
            [this]() { rebuild(); });
  }

  rebuild();
}

void ConductorBoard::setDependencies(DependencyGraph *graph) {
  m_dependencies = graph;
  renderDependencyGraph();
}

void ConductorBoard::setSessionFolder(const QString &folder) {
  m_sessionFolder = folder;
}

QWidget *ConductorBoard::buildGraphTab() {
  m_graphPanel = new QWidget(this);
  m_graphPanel->setObjectName(QStringLiteral("conductorGraphPanel"));

  auto *layout = new QVBoxLayout(m_graphPanel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_graphToolbar = new DiagramToolbar(m_graphPanel);
  layout->addWidget(m_graphToolbar);

  m_graphView = new DiagramView(m_graphPanel);
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

  return m_graphPanel;
}

QWidget *ConductorBoard::buildKanbanTab() {
  m_kanbanScroll = new QScrollArea(this);
  m_kanbanScroll->setWidgetResizable(true);
  m_kanbanScroll->setFrameShape(QFrame::NoFrame);

  auto *host = new QWidget(m_kanbanScroll);

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

  m_kanbanScroll->setWidget(host);

  return m_kanbanScroll;
}

void ConductorBoard::rebuild() {
  if (!m_queue || !m_kanbanScroll)
    return;

  auto *host = m_kanbanScroll->widget();

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

      if (QWidget *w = item->widget()) {
        w->setParent(nullptr);
        w->deleteLater();
      }

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

      if (!req.blockedOn.isEmpty()) {
        QStringList blockers;

        for (const QString &b : req.blockedOn)
          blockers.append(nodeLabelFor(b));

        auto *blocked = new QLabel(
            tr("Waiting on %1").arg(blockers.join(QStringLiteral(", "))),
            card);
        blocked->setWordWrap(true);
        blocked->setStyleSheet(
            QStringLiteral("color: %1;").arg(tokens.textMuted.name()));
        cardLayout->addWidget(blocked);
      }

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

      if (!req.workerId.isEmpty()) {
        auto *worker = new QLabel(workerLabelFor(req.workerId), card);
        worker->setWordWrap(true);
        worker->setStyleSheet(
            QStringLiteral("color: %1;").arg(tokens.textSubtle.name()));
        cardLayout->addWidget(worker);
      }

      auto *actions = new QHBoxLayout;
      actions->setContentsMargins(0, 0, 0, 0);

      auto *detail = new QPushButton(tr("Open"), card);
      connect(detail, &QPushButton::clicked, this,
              [this, requestId = req.id]() { showDetail(requestId); });
      actions->addWidget(detail);

      const bool failed = req.state == QStringLiteral("failed");
      const bool skipped = req.state == QStringLiteral("skipped");
      const bool rejected = req.state == QStringLiteral("rejected");
      const bool done = req.state == QStringLiteral("done");

      if (failed) {
        auto *retry = new QPushButton(tr("Retry"), card);
        connect(retry, &QPushButton::clicked, this,
                [this, requestId = req.id]() {
                  emit retryRequested(requestId);
                });
        actions->addWidget(retry);

        auto *skip = new QPushButton(tr("Skip"), card);
        connect(skip, &QPushButton::clicked, this,
                [this, requestId = req.id]() {
                  emit skipRequested(requestId);
                });
        actions->addWidget(skip);

        auto *remove = new QPushButton(tr("Remove"), card);
        connect(remove, &QPushButton::clicked, this,
                [this, requestId = req.id]() {
                  emit removeRequested(requestId);
                });
        actions->addWidget(remove);
      } else if (!done && !skipped && !rejected) {
        auto *cancel = new QPushButton(tr("Reject"), card);
        connect(cancel, &QPushButton::clicked, this,
                [this, requestId = req.id]() {
                  emit cancelRequested(requestId);
                });
        actions->addWidget(cancel);
      }

      actions->addStretch(1);

      cardLayout->addLayout(actions);

      columnLayout->insertWidget(columnLayout->count() - 1, card);

      break;
    }
  }

  renderDependencyGraph();
}

QString ConductorBoard::nodeLabelFor(const QString &requestId) const {
  if (m_queue) {
    const ConductorRequest req = m_queue->byId(requestId);

    if (!req.text.isEmpty()) {
      QString label = req.text.simplified();

      if (label.size() > 48)
        label = label.left(45) + QStringLiteral("…");

      return label;
    }
  }

  return tr("(unlabelled)");
}

QString ConductorBoard::workerLabelFor(const QString &workerId) const {
  if (workerId.isEmpty())
    return {};

  if (m_roster) {
    const ConductorWorker w = m_roster->byId(workerId);

    if (!w.id.isEmpty()) {
      if (w.type == QStringLiteral("file")) {
        return w.domain.isEmpty() ? tr("worker") : w.domain;
      }

      const QString file = QFileInfo(w.file).fileName();

      if (!file.isEmpty())
        return file;
    }
  }

  return tr("worker");
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

    dot += QStringLiteral("  \"%1\" [label=\"%2\"];\n")
               .arg(id, dotEscape(nodeLabelFor(id)));
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
    auto *worker = new QLabel(
        tr("Worker: %1").arg(workerLabelFor(req.workerId)), panel);
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