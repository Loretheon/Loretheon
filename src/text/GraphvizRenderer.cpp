#include "../../include/text/GraphvizRenderer.h"

#include "SvgThemer.h"

#include <QProcess>
#include <QStringList>

#ifndef EPISTEME_GRAPHVIZ_DOT
#define EPISTEME_GRAPHVIZ_DOT "dot"
#endif

namespace {

QString graphvizExecutable() {
  const QByteArray env = qgetenv("QF_GRAPHVIZ_DOT");

  if (!env.isEmpty()) {
    return QString::fromLocal8Bit(env);
  }

  return QStringLiteral(EPISTEME_GRAPHVIZ_DOT);
}

QString fmtName(GraphvizRenderer::OutputFormat f) {
  return f == GraphvizRenderer::OutputFormat::PNG ? "png" : "pdf";
}

QString trimErr(const QByteArray &err, int code) {
  const QString s = QString::fromUtf8(err).trimmed();
  return s.isEmpty() ? QString("Graphviz exited with code %1").arg(code) : s;
}

} // namespace

GraphvizRenderer::GraphvizRenderer(QObject *parent) : QObject(parent) {}

GraphvizRenderer::~GraphvizRenderer() = default;

void GraphvizRenderer::setThemeTokens(const ThemeTokens &tokens) {
  m_tokens = tokens;
}

QString GraphvizRenderer::renderToSvg(const QString &dotSource,
                                      QString &errorMessage) {
  const QByteArray out = runDot(dotSource.toUtf8(), "svg", errorMessage);
  if (!errorMessage.isEmpty()) return QString();
  return SvgThemer::applyTheme(QString::fromUtf8(out), m_tokens);
}

QByteArray GraphvizRenderer::renderToImage(const QString &dotSource,
                                           OutputFormat format,
                                           QString &errorMessage) {
  const QString fmt = (format == OutputFormat::PNG) ? "png" : "pdf";
  return runDot(dotSource.toUtf8(), fmt, errorMessage);
}

bool GraphvizRenderer::isGraphvizAvailable() {
  QProcess p;
  p.start(graphvizExecutable(), {"-V"});
  return p.waitForFinished(2000) && p.exitCode() == 0;
}

QByteArray GraphvizRenderer::runDot(const QByteArray &input,
                                    const QString &format,
                                    QString &errorMessage) {
  errorMessage.clear();

  QProcess p;
  p.start(graphvizExecutable(), {"-T" + format});
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

void GraphvizRenderer::renderToSvgAsync(const QString &dotSource) {
  auto *p = new QProcess(this);
  p->setProgram(graphvizExecutable());
  p->setArguments({"-Tsvg"});

  connect(p, &QProcess::errorOccurred, this,
          [this, p](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
              emit renderFailed("Failed to start Graphviz. Is 'dot' on PATH?");
              p->deleteLater();
            }
          });

  connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this, p](int code, QProcess::ExitStatus status) {
            const QByteArray out = p->readAllStandardOutput();
            const QByteArray err = p->readAllStandardError();
            p->deleteLater();
            if (status != QProcess::NormalExit || code != 0) {
              emit renderFailed(trimErr(err, code));
              return;
            }
            emit svgReady(SvgThemer::applyTheme(QString::fromUtf8(out), m_tokens));
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
  p->setProgram(graphvizExecutable());
  p->setArguments({"-T" + fmtName(format)});

  connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this, p, format](int code, QProcess::ExitStatus status) {
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