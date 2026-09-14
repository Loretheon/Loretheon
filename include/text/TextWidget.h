#ifndef TEXTWIDGET_H
#define TEXTWIDGET_H

#include <QStackedWidget>
#include <QTabWidget>

#include "DiagramDocument.h"
#include "DiagramToolbar.h"
#include "DiagramView.h"
#include "GraphvizRenderer.h"
#include "PlantUmlRenderer.h"
#include "MermaidRenderer.h"
#include "TextBrowser.h"
#include "TextDocument.h"
#include "TextEdit.h"


class TextWidget : public QTabWidget {
  Q_OBJECT

public:
  explicit TextWidget(QWidget *parent = nullptr);

  void setProjectRoot(const QString &root);
  void setActiveDocument(TextDocument *document);
  TextEdit *editor() const { return textEdit; }
  signals:
    void statusMessage(const QString &message, int timeout = 0);
  void openDocumentRequested(const QString &path);

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
};

#endif // TEXTWIDGET_H