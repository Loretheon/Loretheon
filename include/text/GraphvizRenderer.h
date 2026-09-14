#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include "ThemeAware.h"

class GraphvizRenderer : public QObject, public ThemeAware {
  Q_OBJECT
public:
  enum class OutputFormat { PNG, PDF };
  Q_ENUM(OutputFormat)

  explicit GraphvizRenderer(QObject *parent = nullptr);
  ~GraphvizRenderer() override;

  QString renderToSvg(const QString &dotSource, QString &errorMessage);
  QByteArray renderToImage(const QString &dotSource, OutputFormat format,
                           QString &errorMessage);

  void renderToSvgAsync(const QString &dotSource);
  void renderToImageAsync(const QString &dotSource, OutputFormat format);

  static bool isGraphvizAvailable();

  void setThemeTokens(const ThemeTokens &tokens) override;

  signals:
    void svgReady(const QString &themedSvg);
  void imageReady(const QByteArray &bytes,
                  GraphvizRenderer::OutputFormat format);
  void renderFailed(const QString &errorMessage);

private:
  QByteArray runDot(const QByteArray &input, const QString &format,
                    QString &errorMessage);

  ThemeTokens m_tokens;
};