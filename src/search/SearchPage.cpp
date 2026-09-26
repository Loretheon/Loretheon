#include "../../include/search/SearchPage.h"

#include "inference/InferenceService.h"
#include "../../include/search/RetrievalLoop.h"
#include "../../include/search/SearchService.h"

#include <QCheckBox>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kDebounceMs = 400;
constexpr int kMaxResults = 20;

} // namespace

SearchPage::SearchPage(SearchService *search,
                       InferenceService *inference,
                       QWidget *parent)
    : QWidget(parent), m_search(search), m_inference(inference) {
  setObjectName(QStringLiteral("searchPage"));

  m_query = new QLineEdit(this);
  m_query->setObjectName(QStringLiteral("searchQuery"));
  m_query->setPlaceholderText(tr("Ask about your notes"));
  m_query->setClearButtonEnabled(true);
  m_query->setMinimumHeight(38);

  m_answer = new QTextBrowser(this);
  m_answer->setObjectName(QStringLiteral("searchAnswer"));
  m_answer->setOpenExternalLinks(true);

  m_plainToggle = new QCheckBox(tr("Show raw results"), this);
  m_plainToggle->setObjectName(QStringLiteral("searchPlainToggle"));
  m_plainToggle->setChecked(false);

  m_results = new QListWidget(this);
  m_results->setObjectName(QStringLiteral("searchResults"));
  m_results->setAlternatingRowColors(true);
  m_results->setWordWrap(true);
  m_results->setVisible(false);

  m_status = new QLabel(this);
  m_status->setObjectName(QStringLiteral("searchStatus"));
  m_status->setWordWrap(true);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(80, 40, 80, 24);
  layout->setSpacing(12);
  layout->addWidget(m_query);
  layout->addWidget(m_answer, 1);
  layout->addWidget(m_plainToggle);
  layout->addWidget(m_results, 1);
  layout->addWidget(m_status);

  m_debounce = new QTimer(this);
  m_debounce->setSingleShot(true);
  m_debounce->setInterval(kDebounceMs);

  if (m_search && m_inference) {
    m_loop = new RetrievalLoop(m_search, m_inference, this);

    connect(m_loop, &RetrievalLoop::stageChanged, this,
            &SearchPage::onStageChanged);
    connect(m_loop, &RetrievalLoop::answerChunk, this,
            &SearchPage::onAnswerChunk);
    connect(m_loop, &RetrievalLoop::finished, this,
            &SearchPage::onAnswerFinished);
    connect(m_loop, &RetrievalLoop::sourcesUpdated, this,
            &SearchPage::onSourcesUpdated);
    connect(m_loop, &RetrievalLoop::failed, this,
            &SearchPage::onLoopFailed);
  }

  connect(m_debounce, &QTimer::timeout, this, &SearchPage::onReturnPressed);
  connect(m_query, &QLineEdit::textChanged, this,
          &SearchPage::onQueryChanged);
  connect(m_query, &QLineEdit::returnPressed, this,
          &SearchPage::onReturnPressed);
  connect(m_results, &QListWidget::itemActivated, this,
          &SearchPage::onItemActivated);
  connect(m_plainToggle, &QCheckBox::toggled, this,
          &SearchPage::onTogglePlainList);

  m_status->setText(tr("Ask a question."));
}

SearchPage::~SearchPage() = default;

void SearchPage::focusQuery() {
  m_query->setFocus(Qt::OtherFocusReason);
  m_query->selectAll();
}

void SearchPage::onQueryChanged() {
  m_debounce->start();
}

void SearchPage::onReturnPressed() {
  m_debounce->stop();

  if (m_plainToggle->isChecked()) {
    runPlainSearch();
  } else {
    runLoop();
  }
}

void SearchPage::runLoop() {
  if (!m_loop) {
    m_status->setText(tr("Search is unavailable."));
    return;
  }

  const QString query = m_query->text().trimmed();

  if (query.isEmpty()) {
    m_answer->clear();
    m_results->clear();
    m_status->setText(tr("Ask a question."));
    return;
  }

  m_answerBuffer.clear();
  m_answer->clear();
  m_results->clear();

  m_loop->start(query);
}

void SearchPage::runPlainSearch() {
  m_results->clear();

  if (!m_search || !m_search->isReady()) {
    m_status->setText(tr("Index is not ready."));
    return;
  }

  const QString query = m_query->text().trimmed();

  if (query.isEmpty()) {
    m_status->setText(tr("Type to search."));
    return;
  }

  const QVector<SearchHit> hits = m_search->search(query, kMaxResults);

  for (const SearchHit &hit : hits) {
    const QString heading =
        hit.heading.isEmpty() ? QFileInfo(hit.filePath).fileName()
                              : hit.heading;

    auto *item = new QListWidgetItem(m_results);
    item->setText(QStringLiteral("%1\n%2\n%3")
                      .arg(heading,
                           snippetFor(hit.body, 180),
                           hit.filePath));
    item->setData(Qt::UserRole, hit.filePath);
    item->setData(Qt::UserRole + 1, hit.scopeId);
  }

  m_status->setText(tr("%1 result(s).").arg(hits.size()));
}

void SearchPage::onTogglePlainList(bool enabled) {
  m_results->setVisible(enabled);
  m_answer->setVisible(!enabled);

  if (enabled) {
    runPlainSearch();
  } else if (!m_query->text().trimmed().isEmpty()) {
    runLoop();
  }
}

void SearchPage::onStageChanged(const QString &label) {
  m_status->setText(label);
}

void SearchPage::onAnswerChunk(const QString &text) {
  m_answerBuffer += text;
  renderAnswer();
}

void SearchPage::onAnswerFinished(const QString &answer) {
  m_answerBuffer = answer;
  renderAnswer();
  m_status->setText(tr("Answered."));
}

void SearchPage::renderAnswer() {
  m_answer->setMarkdown(m_answerBuffer);
  m_answer->moveCursor(QTextCursor::End);
}

void SearchPage::onSourcesUpdated(const QVector<SearchHit> &hits) {
  m_results->clear();

  for (const SearchHit &hit : hits) {
    const QString heading =
        hit.heading.isEmpty() ? QFileInfo(hit.filePath).fileName()
                              : hit.heading;

    auto *item = new QListWidgetItem(m_results);
    item->setText(QStringLiteral("%1\n%2\n%3")
                      .arg(heading,
                           snippetFor(hit.body, 180),
                           hit.filePath));
    item->setData(Qt::UserRole, hit.filePath);
    item->setData(Qt::UserRole + 1, hit.scopeId);
  }
}

void SearchPage::onLoopFailed(const QString &reason) {
  m_answerBuffer = QStringLiteral("> ") + reason;
  renderAnswer();
  m_status->setText(reason);
}

void SearchPage::onItemActivated() {
  auto *item = m_results->currentItem();

  if (!item) {
    return;
  }

  const QString path = item->data(Qt::UserRole).toString();
  const QString scope = item->data(Qt::UserRole + 1).toString();

  if (!path.isEmpty()) {
    emit openRequested(path, scope);
  }
}

QString SearchPage::snippetFor(const QString &body, int maxLength) const {
  QString flat = body;
  flat.replace(QRegularExpression(QStringLiteral("\\s+")),
               QStringLiteral(" "));
  flat = flat.trimmed();

  if (flat.length() <= maxLength) {
    return flat;
  }

  return flat.left(maxLength - 1) + QStringLiteral("…");
}

void SearchPage::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    if (!m_query->text().isEmpty()) {
      m_query->clear();
      m_answer->clear();
      m_results->clear();
      m_status->setText(tr("Ask a question."));
    } else {
      window()->setFocus(Qt::OtherFocusReason);
    }
    event->accept();
    return;
  }

  QWidget::keyPressEvent(event);
}