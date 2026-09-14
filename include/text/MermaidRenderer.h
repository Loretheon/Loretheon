#ifndef MERMAID_RENDERER_H
#define MERMAID_RENDERER_H

#include <QObject>
#include <QString>

class QProcess;

class MermaidRenderer : public QObject {
  Q_OBJECT

public:
  explicit MermaidRenderer(QObject *parent = nullptr);
  ~MermaidRenderer();

  void renderToSvgAsync(const QString &mermaidSource);
  static bool isMermaidAvailable();

  signals:
    void svgReady(const QString &svg);
  void renderFailed(const QString &errorMessage);

private:
  QString runMermaid(const QString &source, QString &errorMessage);
};

#endif // MERMAID_RENDERER_H