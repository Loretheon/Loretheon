#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class SearchService;

// Minimal query window. Type a question, see ranked hits. Double-click
// a hit to open the file at the scope.
//
// Not the final UI. This exists so the retrieval pipeline can be
// exercised without any editor integration.
class SearchDialog : public QDialog {
  Q_OBJECT

public:
  explicit SearchDialog(SearchService *service,
                        QWidget *parent = nullptr);

  signals:
    // Emitted when the user activates a hit. Carries the file path and
    // the scope id. The receiver decides what to do with them.
    void openRequested(const QString &filePath, const QString &scopeId);

private slots:
  void onQueryChanged();
  void onItemActivated();

private:
  QString snippetFor(const QString &body, int maxLength) const;

  SearchService *m_service = nullptr;

  QLineEdit *m_query = nullptr;
  QListWidget *m_results = nullptr;
  QLabel *m_status = nullptr;
  QPushButton *m_close = nullptr;
};