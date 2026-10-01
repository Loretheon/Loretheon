#ifndef EPISTEME_DOCUMENTAREA_H
#define EPISTEME_DOCUMENTAREA_H

#include <QHash>
#include <QStackedWidget>
#include <QTabWidget>

#include "ThemeAware.h"

class DocumentManager;
class TextDocument;
class TextEdit;
class TextWidget;
class EditSession;
class MediaPane;

class QToolButton;

class DocumentArea : public QStackedWidget {
  Q_OBJECT

public:
  explicit DocumentArea(DocumentManager *manager, QWidget *parent = nullptr);

  TextEdit *currentEditor() const;
  TextWidget *currentTextWidget() const;
  TextDocument *currentDocument() const;

  void setEditSession(EditSession *session);

public slots:
  void setThemeTokens(const ThemeTokens &tokens);

  void showMediaFile(const QString &absolutePath);

  void showTextTabs();

  signals:
    void currentEditorChanged(TextEdit *editor);
  void currentTextWidgetChanged(TextWidget *widget);

  void openDocumentRequested(const QString &path);
  void statusMessage(const QString &text, int timeoutMs);

private slots:
  void onDocumentOpened(TextDocument *document);
  void onDocumentClosed(TextDocument *document);
  void onCurrentChanged(int index);

private:
  TextWidget *widgetForDocument(TextDocument *document) const;
  int indexForDocument(TextDocument *document) const;

  void wireSessionToEditor(EditSession *session, TextEdit *editor);

  void installTabButtons(TextWidget *page);
  void syncTabButtons();

  DocumentManager *m_manager = nullptr;
  EditSession *m_session = nullptr;
  TextEdit *m_sessionEditor = nullptr;

  QTabWidget *m_tabs = nullptr;
  MediaPane *m_mediaPane = nullptr;

  QHash<TextDocument *, TextWidget *> m_widgets;

  ThemeTokens m_tokens;

  bool m_syncing = false;

  int m_pageTabs = 0;
  int m_pageMedia = 1;
};

#endif // EPISTEME_DOCUMENTAREA_H