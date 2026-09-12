// GraphvizRenderer.h
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

/**
 * GraphvizRenderer
 * -----------------
 * Runs `dot` and returns either a themed SVG string or raw PNG/PDF bytes.
 *
 * Threading contract:
 *   - The *Async methods are safe to call from the GUI thread.
 *   - The sync methods block. Call them from a worker thread, or accept
 *     a UI stall while `dot` runs.
 *
 * Color contract:
 *   - optimizeGraphvizSvg strips Graphviz's baked-in fill/stroke attributes
 *     and injects a Catppuccin Frappé stylesheet, so every painted element
 *     takes its color from the theme, not from `dot`'s defaults.
 *
 * Sizing contract:
 *   - The returned SVG keeps its viewBox, drops fixed width/height,
 *     sets preserveAspectRatio="xMidYMid meet", and fills 100% of its
 *     container. It will never stretch. The parent widget decides size.
 */
class GraphvizRenderer : public QObject {
  Q_OBJECT
public:
  enum class OutputFormat { PNG, PDF };

  explicit GraphvizRenderer(QObject *parent = nullptr);
  ~GraphvizRenderer() override;

  // ---- Synchronous (blocks the calling thread) --------------------------
  QString    renderToSvg(const QString &dotSource, QString &errorMessage);
  QByteArray renderToImage(const QString &dotSource,
                           OutputFormat format,
                           QString &errorMessage);

  // ---- Asynchronous (safe on the GUI thread) ----------------------------
  void renderToSvgAsync(const QString &dotSource);
  void renderToImageAsync(const QString &dotSource, OutputFormat format);

  // ---- Capability probe -------------------------------------------------
  static bool isGraphvizAvailable();

  // ---- Theming ----------------------------------------------------------
  // bgColor / textColor empty  ->  use Catppuccin Frappé defaults.
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
                    const QString  &format,
                    QString        &errorMessage);
};