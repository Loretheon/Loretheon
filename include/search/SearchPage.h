#pragma once

#include "SearchHit.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextBrowser;
class QTimer;

class InferenceService;
class RetrievalLoop;
class SearchService;
class TextBrowser;

// The @Lore mode. By default shows an LLM-generated answer at the top
// of the page, with the retrieved sources underneath. A toggle reveals
// the plain list of search hits, for when you want to see what the
// retrieval actually returned.
class SearchPage : public QWidget {
  Q_OBJECT

public:
  SearchPage(SearchService *search,
             InferenceService *inference,
             QWidget *parent = nullptr);
  ~SearchPage() override;

  void focusQuery();

  signals:
    void openRequested(const QString &filePath, const QString &scopeId);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void onQueryChanged();
  void onReturnPressed();
  void onItemActivated();
  void onTogglePlainList(bool enabled);
  void onAnswerChunk(const QString &text);
  void onAnswerFinished(const QString &answer);
  void onStageChanged(const QString &label);
  void onSourcesUpdated(const QVector<SearchHit> &hits);
  void onLoopFailed(const QString &reason);

private:
  void runPlainSearch();
  void runLoop();
  void renderAnswer();
  QString snippetFor(const QString &body, int maxLength) const;

  SearchService *m_search = nullptr;
  InferenceService *m_inference = nullptr;
  RetrievalLoop *m_loop = nullptr;

  QLineEdit *m_query = nullptr;

  QTextBrowser *m_answer = nullptr;
  QLabel *m_status = nullptr;

  QCheckBox *m_plainToggle = nullptr;
  QListWidget *m_results = nullptr;

  QTimer *m_debounce = nullptr;

  QString m_answerBuffer;
};