#ifndef EPISTEME_DOCUMENTAREA_H
#define EPISTEME_DOCUMENTAREA_H

#include <QHash>
#include <QTabWidget>

#include "ThemeAware.h"

class DocumentManager;
class TextDocument;
class TextEdit;
class TextWidget;
class EditSession;

class DocumentArea : public QTabWidget {
  Q_OBJECT

public:
  explicit DocumentArea(DocumentManager *manager, QWidget *parent = nullptr);

  TextEdit *currentEditor() const;
  TextWidget *currentTextWidget() const;
  TextDocument *currentDocument() const;

  void setEditSession(EditSession *session);

public slots:
  void setThemeTokens(const ThemeTokens &tokens);

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

  DocumentManager *m_manager = nullptr;
  EditSession *m_session = nullptr;

  QHash<TextDocument *, TextWidget *> m_widgets;

  ThemeTokens m_tokens;

  bool m_syncing = false;
};

#endif // EPISTEME_DOCUMENTAREA_H