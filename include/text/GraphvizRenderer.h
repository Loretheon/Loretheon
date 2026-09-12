#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class GraphvizRenderer : public QObject {
  Q_OBJECT
public:
  enum class OutputFormat { PNG, PDF };

  explicit GraphvizRenderer(QObject *parent = nullptr);
  ~GraphvizRenderer() override;

  QString    renderToSvg(const QString &dotSource, QString &errorMessage);
  QByteArray renderToImage(const QString &dotSource,
                           OutputFormat format,
                           QString &errorMessage);

  void renderToSvgAsync(const QString &dotSource);
  void renderToImageAsync(const QString &dotSource, OutputFormat format);

  static bool isGraphvizAvailable();

  static QString optimizeGraphvizSvg(const QString &rawSvg,
                                     const QString &bgColor = QString(),
                                     const QString &textColor = QString());

  signals:
    void svgReady(const QString &themedSvg);
  void imageReady(const QByteArray &bytes,
                  GraphvizRenderer::OutputFormat format);
  void renderFailed(const QString &errorMessage);

private:
  QByteArray runDot(const QByteArray &input,
                    const QString    &format,
                    QString          &errorMessage);
};