#ifndef MERMAID_RENDERER_H
#define MERMAID_RENDERER_H

#include <QObject>
#include <QString>

#include "ThemeAware.h"

class QProcess;

class MermaidRenderer : public QObject, public ThemeAware {
  Q_OBJECT

public:
  explicit MermaidRenderer(QObject *parent = nullptr);
  ~MermaidRenderer();

  void renderToSvgAsync(const QString &mermaidSource);
  static bool isMermaidAvailable();

  void setThemeTokens(const ThemeTokens &tokens) override;

  signals:
    void svgReady(const QString &svg);
  void renderFailed(const QString &errorMessage);

private:
  QString runMermaid(const QString &source, QString &errorMessage);
  QString writeMermaidConfig(const QString &dirPath) const;

  ThemeTokens m_tokens;
};

#endif // MERMAID_RENDERER_H