#ifndef EPISTEME_TEXTWIDGET_H
#define EPISTEME_TEXTWIDGET_H

#include "GraphvizRenderer.h"

#include <QStackedWidget>
#include <QTabWidget>
#include <QTextDocument>

#include "TextBrowser.h"
#include "TextDocument.h"
#include "TextEdit.h"

class DiagramDocument;
class DiagramToolbar;
class DiagramView;

class TextWidget : public QTabWidget {
  Q_OBJECT

public:
  explicit TextWidget(QWidget *parent = nullptr);

  TextEdit *editor() const { return textEdit; }
  TextBrowser *browser() const { return textBrowser; }
  DiagramView *diagram() const { return svgView; }
  DiagramDocument *diagramDocument() const { return diagramDoc; }

  void setProjectRoot(const QString &root);

  signals:
    void openDocumentRequested(const QString &path);
  void statusMessage(const QString &text, int timeoutMs = 3000);

public slots:
  void setActiveDocument(TextDocument *newDocument);

private slots:
  void syncPreview();
  void disconnectActiveDocument();
  void clearPreview();
  void onElementClicked(const QString &id, const QString &name,
                        const QPoint &globalPos);
private:
  void findInEditor(const QString &text);
  void showNodeContextMenu(const QString &id,
                           const QString &name,
                           const QPoint &globalPos);

  QStackedWidget *viewStack = nullptr;
  QWidget *diagramPage = nullptr;
  TextBrowser *textBrowser = nullptr;
  TextEdit *textEdit = nullptr;
  DiagramView *svgView = nullptr;
  DiagramToolbar *diagramToolbar = nullptr;
  DiagramDocument *diagramDoc = nullptr;
  QTextDocument *previewDocument = nullptr;
  GraphvizRenderer *graphvizRenderer = nullptr;
  TextDocument *activeDocument = nullptr;

  QString errorMessage;
};

#endif // EPISTEME_TEXTWIDGET_H