#include "../../include/search/SearchDialog.h"

#include "../../include/search/SearchService.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kDebounceMs = 250;
constexpr int kMaxResults = 12;

} // namespace

SearchDialog::SearchDialog(SearchService *service, QWidget *parent)
    : QDialog(parent), m_service(service) {
  setWindowTitle(tr("Search"));
  resize(560, 480);

  m_query = new QLineEdit(this);
  m_query->setPlaceholderText(tr("Ask a question or type keywords"));
  m_query->setClearButtonEnabled(true);

  m_results = new QListWidget(this);
  m_results->setAlternatingRowColors(true);

  m_status = new QLabel(this);
  m_status->setWordWrap(true);

  m_close = new QPushButton(tr("Close"), this);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  buttons->addWidget(m_close);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(m_query);
  layout->addWidget(m_results, 1);
  layout->addWidget(m_status);
  layout->addLayout(buttons);

  connect(m_query, &QLineEdit::textChanged, this,
          &SearchDialog::onQueryChanged);
  connect(m_results, &QListWidget::itemActivated, this,
          &SearchDialog::onItemActivated);
  connect(m_close, &QPushButton::clicked, this, &QDialog::close);
}

void SearchDialog::onQueryChanged() {
  QTimer::singleShot(kDebounceMs, this, [this]() {
    m_results->clear();

    if (!m_service) {
      m_status->setText(tr("Search is unavailable."));
      return;
    }

    if (!m_service->isReady()) {
      m_status->setText(tr("Index is not ready."));
      return;
    }

    const QString query = m_query->text().trimmed();

    if (query.isEmpty()) {
      m_status->setText(tr("Type a query."));
      return;
    }

    const QVector<SearchHit> hits =
        m_service->search(query, kMaxResults);

    if (hits.isEmpty()) {
      m_status->setText(tr("No results."));
      return;
    }

    for (const SearchHit &hit : hits) {
      const QString heading =
          hit.heading.isEmpty() ? tr("(no heading)") : hit.heading;

      auto *item = new QListWidgetItem(m_results);
      item->setText(QStringLiteral("%1\n%2\n%3")
                        .arg(heading,
                             snippetFor(hit.body, 160),
                             QFileInfo(hit.filePath).fileName()));
      item->setData(Qt::UserRole, hit.filePath);
      item->setData(Qt::UserRole + 1, hit.scopeId);
    }

    m_status->setText(
        tr("%1 result(s).").arg(hits.size()));
  });
}

void SearchDialog::onItemActivated() {
  auto *item = m_results->currentItem();

  if (!item) {
    return;
  }

  const QString path = item->data(Qt::UserRole).toString();
  const QString scope = item->data(Qt::UserRole + 1).toString();

  emit openRequested(path, scope);
}

QString SearchDialog::snippetFor(const QString &body,
                                 int maxLength) const {
  QString flat = body;
  flat.replace(QRegularExpression(QStringLiteral("\\s+")),
               QStringLiteral(" "));

  if (flat.length() <= maxLength) {
    return flat;
  }

  return flat.left(maxLength - 1) + QStringLiteral("…");
}