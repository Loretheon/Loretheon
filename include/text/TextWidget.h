// TextWidget.h
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

public slots:
  void setActiveDocument(TextDocument *newDocument);

private slots:
  void syncPreview();
  void disconnectActiveDocument();
  void clearPreview();

private:
  void findInEditor(const QString &text);

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