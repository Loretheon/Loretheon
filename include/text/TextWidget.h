#ifndef TEXTWIDGET_H
#define TEXTWIDGET_H

#include <QTabWidget>
#include <QString>
#include <QPoint>

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

class TextWidget : public QTabWidget, public ThemeAware {
  Q_OBJECT

public:
  explicit TextWidget(QWidget *parent = nullptr);

  TextEdit *editor() const { return textEdit; }
  void setActiveDocument(TextDocument *document);
  void setProjectRoot(const QString &root);

  void setThemeTokens(const ThemeTokens &tokens) override;


public slots:
void setContextScopes(const QStringList &scopeIds);

  
  signals:
    void openDocumentRequested(const QString &path);
  void statusMessage(const QString &text, int timeoutMs = 4000);

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

  TextEdit *textEdit;
  TextBrowser *textBrowser;
  QStackedWidget *viewStack;
  QWidget *diagramPage;
  DiagramView *svgView;
  DiagramToolbar *diagramToolbar;
  DiagramDocument *diagramDoc;
  QTextDocument *previewDocument;
  GraphvizRenderer *graphvizRenderer;
  PlantUmlRenderer *plantUmlRenderer;
  MermaidRenderer *mermaidRenderer;

  TextDocument *activeDocument = nullptr;
  ThemeTokens m_tokens;
};

#endif // TEXTWIDGET_H