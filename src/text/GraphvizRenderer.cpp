// GraphvizRenderer.cpp
#include "../../include/text/GraphvizRenderer.h"

#include <QDebug>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

GraphvizRenderer::GraphvizRenderer(QObject *parent) : QObject(parent) {}
GraphvizRenderer::~GraphvizRenderer() = default;

QString GraphvizRenderer::renderToSvg(const QString &dotSource,
                                      QString &errorMessage) {
  const QByteArray out = runDot(dotSource.toUtf8(), "svg", errorMessage);
  if (!errorMessage.isEmpty()) return QString();
  return QString::fromUtf8(out);
}

QByteArray GraphvizRenderer::renderToImage(const QString &dotSource,
                                           OutputFormat format,
                                           QString &errorMessage) {
  const QString fmt = (format == OutputFormat::PNG) ? "png" : "pdf";
  return runDot(dotSource.toUtf8(), fmt, errorMessage);
}

bool GraphvizRenderer::isGraphvizAvailable() {
  QProcess p;
  p.start("dot", {"-V"});
  return p.waitForFinished(2000) && p.exitCode() == 0;
}

QByteArray GraphvizRenderer::runDot(const QByteArray &input,
                                    const QString    &format,
                                    QString          &errorMessage) {
  errorMessage.clear();

  QProcess p;
  p.start("dot", {"-T" + format});
  if (!p.waitForStarted(3000)) {
    errorMessage = "Failed to start Graphviz. Is 'dot' on PATH?";
    return {};
  }

  p.write(input);
  p.closeWriteChannel();

  if (!p.waitForFinished(15000)) {
    p.kill();
    p.waitForFinished(1000);
    errorMessage = "Graphviz rendering timed out";
    return {};
  }

  if (p.exitCode() != 0) {
    errorMessage = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (errorMessage.isEmpty())
      errorMessage = QString("Graphviz exited with code %1").arg(p.exitCode());
    return {};
  }

  return p.readAllStandardOutput();
}

namespace {
QString fmtName(GraphvizRenderer::OutputFormat f) {
  return f == GraphvizRenderer::OutputFormat::PNG ? "png" : "pdf";
}
QString trimErr(const QByteArray &err, int code) {
  const QString s = QString::fromUtf8(err).trimmed();
  return s.isEmpty() ? QString("Graphviz exited with code %1").arg(code) : s;
}
} // namespace

void GraphvizRenderer::renderToSvgAsync(const QString &dotSource) {
  auto *p = new QProcess(this);
  p->setProgram("dot");
  p->setArguments({"-Tsvg"});

  connect(p, &QProcess::errorOccurred, this,
          [this, p](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
              emit renderFailed("Failed to start Graphviz. Is 'dot' on PATH?");
              p->deleteLater();
            }
          });

  connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this,
          [this, p](int code, QProcess::ExitStatus status) {
            const QByteArray out = p->readAllStandardOutput();
            const QByteArray err = p->readAllStandardError();
            p->deleteLater();
            if (status != QProcess::NormalExit || code != 0) {
              emit renderFailed(trimErr(err, code));
              return;
            }
            emit svgReady(QString::fromUtf8(out));
          });

  p->start();
  if (!p->waitForStarted(3000)) {
    p->deleteLater();
    emit renderFailed("Failed to start Graphviz. Is 'dot' on PATH?");
    return;
  }
  p->write(dotSource.toUtf8());
  p->closeWriteChannel();
}

void GraphvizRenderer::renderToImageAsync(const QString &dotSource,
                                          OutputFormat format) {
  auto *p = new QProcess(this);
  p->setProgram("dot");
  p->setArguments({"-T" + fmtName(format)});

  connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this,
          [this, p, format](int code, QProcess::ExitStatus status) {
            const QByteArray out = p->readAllStandardOutput();
            const QByteArray err = p->readAllStandardError();
            p->deleteLater();
            if (status != QProcess::NormalExit || code != 0) {
              emit renderFailed(trimErr(err, code));
              return;
            }
            emit imageReady(out, format);
          });

  p->start();
  if (!p->waitForStarted(3000)) {
    p->deleteLater();
    emit renderFailed("Failed to start Graphviz. Is 'dot' on PATH?");
    return;
  }
  p->write(dotSource.toUtf8());
  p->closeWriteChannel();
}

QString GraphvizRenderer::optimizeGraphvizSvg(const QString &rawSvg,
                                              const QString &bgColor,
                                              const QString &textColor) {
  QDomDocument doc;
  QString parseError;
  int errorLine = 0;
  if (!doc.setContent(rawSvg, false, &parseError, &errorLine)) {
    qWarning() << "SVG parse failed @" << errorLine << parseError;
    return rawSvg;
  }
  QDomElement svg = doc.documentElement();
  if (svg.isNull() || svg.tagName() != "svg") {
    qWarning() << "Root is not <svg>";
    return rawSvg;
  }

  const QString nodeFill   = "#292c3c";
  const QString nodeStroke = "#8caaee";
  const QString edgeStroke = "#949cbb";
  const QString textFill   = "#c6d0f5";

  const QString effNodeFill   = bgColor.isEmpty()   ? nodeFill   : bgColor;
  const QString effNodeStroke = "#8caaee";
  const QString effEdgeStroke = "#949cbb";
  const QString effText       = textColor.isEmpty() ? textFill  : textColor;

  const QStringList colorAttrs = {"fill", "stroke", "stop-color"};
  const QStringList tags = {"svg", "g", "text", "tspan", "polygon",
                            "ellipse", "path", "rect", "circle", "line",
                            "polyline", "use", "image", "title", "a"};

  for (const QString &tag : tags) {
    const QDomNodeList elems = doc.elementsByTagName(tag);
    for (int i = 0; i < elems.count(); ++i) {
      QDomElement el = elems.at(i).toElement();
      if (el.isNull()) continue;

      for (const QString &attr : colorAttrs) {
        if (!el.hasAttribute(attr)) continue;
        const QString v = el.attribute(attr).trimmed().toLower();
        if (v.startsWith('#') || v.startsWith("rgb") ||
            v == "black" || v == "white" ||
            v == "red"   || v == "green" || v == "blue" ||
            v == "gray"  || v == "grey") {
          el.removeAttribute(attr);
        }
      }

      if (el.hasAttribute("style")) {
        QString s = el.attribute("style");
        s.remove(QRegularExpression(
            R"(\b(fill|stroke|stop-color)\s*:\s*[^;]+;?)",
            QRegularExpression::CaseInsensitiveOption));
        s = s.trimmed();
        if (s.isEmpty()) el.removeAttribute("style");
        else             el.setAttribute("style", s);
      }
    }
  }

  const QDomNodeList textNodes = doc.elementsByTagName("text");
  for (int i = 0; i < textNodes.count(); ++i) {
    QDomElement el = textNodes.at(i).toElement();
    if (el.isNull()) continue;
    el.setAttribute("fill", effText);
  }

  const QDomNodeList tspanNodes = doc.elementsByTagName("tspan");
  for (int i = 0; i < tspanNodes.count(); ++i) {
    QDomElement el = tspanNodes.at(i).toElement();
    if (el.isNull()) continue;
    el.setAttribute("fill", effText);
  }

  const QDomNodeList polyNodes = doc.elementsByTagName("polygon");
  for (int i = 0; i < polyNodes.count(); ++i) {
    QDomElement el = polyNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (!el.hasAttribute("fill"))   el.setAttribute("fill", effNodeFill);
    if (!el.hasAttribute("stroke")) el.setAttribute("stroke", effEdgeStroke);
  }

  const QDomNodeList ellipseNodes = doc.elementsByTagName("ellipse");
  for (int i = 0; i < ellipseNodes.count(); ++i) {
    QDomElement el = ellipseNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (!el.hasAttribute("fill"))   el.setAttribute("fill", effNodeFill);
    if (!el.hasAttribute("stroke")) el.setAttribute("stroke", effNodeStroke);
  }

  const QDomNodeList rectNodes = doc.elementsByTagName("rect");
  for (int i = 0; i < rectNodes.count(); ++i) {
    QDomElement el = rectNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (!el.hasAttribute("fill"))   el.setAttribute("fill", effNodeFill);
    if (!el.hasAttribute("stroke")) el.setAttribute("stroke", effNodeStroke);
  }

  const QDomNodeList pathNodes = doc.elementsByTagName("path");
  for (int i = 0; i < pathNodes.count(); ++i) {
    QDomElement el = pathNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (!el.hasAttribute("stroke")) el.setAttribute("stroke", effEdgeStroke);
    if (!el.hasAttribute("fill"))   el.setAttribute("fill", "none");
  }

  const QDomNodeList lineNodes = doc.elementsByTagName("line");
  for (int i = 0; i < lineNodes.count(); ++i) {
    QDomElement el = lineNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (!el.hasAttribute("stroke")) el.setAttribute("stroke", effEdgeStroke);
  }

  const QDomNodeList polylineNodes = doc.elementsByTagName("polyline");
  for (int i = 0; i < polylineNodes.count(); ++i) {
    QDomElement el = polylineNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (!el.hasAttribute("stroke")) el.setAttribute("stroke", effEdgeStroke);
    if (!el.hasAttribute("fill"))   el.setAttribute("fill", "none");
  }

  svg.removeAttribute("width");
  svg.removeAttribute("height");
  svg.removeAttribute("style");
  svg.setAttribute("preserveAspectRatio", "xMidYMid meet");

  return doc.toString(-1);
}