#ifndef TEXTWIDGET_H
#define TEXTWIDGET_H

#include <QString>
#include <QPoint>
#include <QWidget>

#include "DiagramDocument.h"
#include "TextDocument.h"
#include "ThemeAware.h"

class TextEdit;
class TextBrowser;
class QStackedWidget;
class DiagramView;
class DiagramToolbar;
class GraphvizRenderer;
class PlantUmlRenderer;
class MermaidRenderer;
class TextDocument;
class PreviewPane;
class PreviewController;
class EditSession;
class QShowEvent;

class TextWidget : public QWidget, public ThemeAware {
  Q_OBJECT

public:
  explicit TextWidget(QWidget *parent = nullptr);

  TextEdit *editor() const { return textEdit; }

  TextDocument *document() const { return activeDocument; }

  void setActiveDocument(TextDocument *document);
  void setProjectRoot(const QString &root);

  void setPreviewSession(EditSession *session);

  void setWorkstationMode(bool on);

  void setThemeTokens(const ThemeTokens &tokens) override;

  bool isViewMode() const;

public slots:
  void setContextScopes(const QStringList &scopeIds);
  void activatePreview(bool active);
  void clearContextScopes();

  // Toggle the outer stack between the editor page (false) and the
  // preview / view page (true).
  void setViewMode(bool viewMode);

signals:
  void openDocumentRequested(const QString &path);
  void statusMessage(const QString &text, int timeoutMs = 4000);

protected:
  void showEvent(QShowEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
  void syncPreview();
  void onElementClicked(const QString &id, const QString &name,
                        const QPoint &globalPos);

private:
  void disconnectActiveDocument();
  void clearPreview();
  void findInEditor(const QString &text);
  void showNodeContextMenu(const QString &id, const QString &name,
                           const QPoint &globalPos);
  void applyThemeToRenderers();

  TextEdit *textEdit = nullptr;
  PreviewPane *previewPane = nullptr;
  PreviewController *previewController = nullptr;
  QStackedWidget *editorStack = nullptr;

  TextBrowser *textBrowser = nullptr;
  QStackedWidget *viewStack = nullptr;
  QWidget *diagramPage = nullptr;
  DiagramView *svgView = nullptr;
  DiagramToolbar *diagramToolbar = nullptr;
  DiagramDocument *diagramDoc = nullptr;
  QTextDocument *previewDocument = nullptr;
  GraphvizRenderer *graphvizRenderer = nullptr;
  PlantUmlRenderer *plantUmlRenderer = nullptr;
  MermaidRenderer *mermaidRenderer = nullptr;

  QStackedWidget *outerStack = nullptr;

  TextDocument *activeDocument = nullptr;
  ThemeTokens m_tokens;

  bool m_workstationMode = false;
};

#endif // TEXTWIDGET_H