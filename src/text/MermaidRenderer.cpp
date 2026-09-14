#include "../../include/text/MermaidRenderer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrent>

#ifndef EPISTEME_MERMAID_CLI
#define EPISTEME_MERMAID_CLI "mmdc"
#endif

namespace {

QString mermaidErrorMessage(QProcess *process) {
  const QString stderrText =
      QString::fromUtf8(process->readAllStandardError()).trimmed();

  if (!stderrText.isEmpty()) {
    return stderrText;
  }

  const QString stdoutText =
      QString::fromUtf8(process->readAllStandardOutput()).trimmed();

  if (!stdoutText.isEmpty()) {
    return stdoutText;
  }

  return QStringLiteral("Mermaid exited with code %1").arg(process->exitCode());
}

void configureMermaidEnvironment(QProcess &process) {
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

  const QFileInfo mmdcInfo(QStringLiteral(EPISTEME_MERMAID_CLI));

  // If EPISTEME_MERMAID_CLI is an absolute/relative path, add its
  // containing directory to PATH. If it is simply "mmdc", this will
  // resolve to the current directory, which is harmless.
  const QString mmdcDirectory = mmdcInfo.absolutePath();

  QString path = environment.value(QStringLiteral("PATH"));
  const QStringList pathEntries =
      path.split(QDir::listSeparator(), Qt::SkipEmptyParts);

  if (!mmdcDirectory.isEmpty() && !pathEntries.contains(mmdcDirectory)) {
    if (path.isEmpty()) {
      path = mmdcDirectory;
    } else {
      path = mmdcDirectory + QDir::listSeparator() + path;
    }
  }

  environment.insert(QStringLiteral("PATH"), path);

  environment.insert(QStringLiteral("PUPPETEER_DISABLE_HEADLESS_WARNING"),
                     QStringLiteral("true"));

  environment.insert(QStringLiteral("PUPPETEER_SKIP_CHROMIUM_DOWNLOAD"),
                     QStringLiteral("true"));

  process.setProcessEnvironment(environment);
}

QString writeMermaidConfig(const QString &dirPath) {
  const QString path =
      QDir(dirPath).filePath(QStringLiteral("mermaid-config.json"));

  QFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return {};
  }

  static const QByteArray kConfig = QByteArrayLiteral(
     "{\n"
     "  \"htmlLabels\": false,\n"
     "  \"useMaxWidth\": false,\n"
     "  \"theme\": \"base\",\n"
     "  \"flowchart\": {\n"
     "    \"useMaxWidth\": false,\n"
     "    \"curve\": \"linear\"\n"
     "  }\n"
     "}\n");

  if (file.write(kConfig) != kConfig.size()) {
    return {};
  }

  if (!file.flush()) {
    return {};
  }

  file.close();

  return path;
}

bool startMermaid(QProcess &process, const QStringList &arguments,
                  int timeout) {
  process.setProgram(QStringLiteral(EPISTEME_MERMAID_CLI));
  process.setArguments(arguments);

  configureMermaidEnvironment(process);

  process.start();

  return process.waitForStarted(timeout);
}

} // namespace

MermaidRenderer::MermaidRenderer(QObject *parent) : QObject(parent) {}

MermaidRenderer::~MermaidRenderer() = default;

bool MermaidRenderer::isMermaidAvailable() {
  QProcess process;

  if (!startMermaid(process, {QStringLiteral("--version")}, 5000)) {
    return false;
  }

  if (!process.waitForFinished(5000)) {
    process.kill();
    process.waitForFinished(1000);
    return false;
  }

  return process.exitStatus() == QProcess::NormalExit &&
         process.exitCode() == 0;
}

QString MermaidRenderer::runMermaid(const QString &source,
                                    QString &errorMessage) {
  errorMessage.clear();

  QTemporaryDir workDir(
      QDir::temp().filePath(QStringLiteral("mermaid_XXXXXX")));

  workDir.setAutoRemove(true);

  if (!workDir.isValid()) {
    errorMessage = QStringLiteral("Failed to create temporary directory");
    return {};
  }

  const QString configPath = writeMermaidConfig(workDir.path());

  if (configPath.isEmpty()) {
    errorMessage = QStringLiteral("Failed to write Mermaid config");
    return {};
  }

  const QString inputPath =
      QDir(workDir.path()).filePath(QStringLiteral("input.mmd"));

  const QString outputPath =
      QDir(workDir.path()).filePath(QStringLiteral("output.svg"));

  // Write Mermaid source.
  {
    QFile inputFile(inputPath);

    if (!inputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      errorMessage = QStringLiteral("Failed to create temporary input file");
      return {};
    }

    const QByteArray sourceData = source.toUtf8();

    if (inputFile.write(sourceData) != sourceData.size()) {
      errorMessage = QStringLiteral("Failed to write Mermaid source");
      return {};
    }

    if (!inputFile.flush()) {
      errorMessage = QStringLiteral("Failed to flush Mermaid source");
      return {};
    }
  }

  // Run Mermaid CLI.
  QProcess process;

  const QStringList arguments = {
      QStringLiteral("-i"), inputPath,

      QStringLiteral("-o"), outputPath,

      QStringLiteral("-c"), configPath,

      QStringLiteral("-b"), QStringLiteral("transparent"),
  };

  if (!startMermaid(process, arguments, 5000)) {
    errorMessage = QStringLiteral("Failed to start Mermaid CLI: %1")
                       .arg(QStringLiteral(EPISTEME_MERMAID_CLI));

    return {};
  }

  // Wait for Mermaid to finish.
  if (!process.waitForFinished(30000)) {
    process.kill();
    process.waitForFinished(1000);

    errorMessage = QStringLiteral("Mermaid rendering timed out");

    return {};
  }

  // Check process result.
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    errorMessage = mermaidErrorMessage(&process);

    return {};
  }

  // Read generated SVG.
  QFile svgFile(outputPath);

  if (!svgFile.open(QIODevice::ReadOnly)) {
    errorMessage =
        QStringLiteral("Failed to read generated SVG: %1").arg(outputPath);

    return {};
  }

  const QByteArray svgData = svgFile.readAll();

  if (svgData.isEmpty()) {
    errorMessage = QStringLiteral("Mermaid generated an empty SVG");

    return {};
  }

  const QString svg = QString::fromUtf8(svgData);

  if (svg.isEmpty()) {
    errorMessage = QStringLiteral("Mermaid generated an empty SVG");

    return {};
  }

  return svg;
}

void MermaidRenderer::renderToSvgAsync(const QString &mermaidSource) {
  /*
   * Keep the actual Mermaid implementation in ONE place:
   * runMermaid().
   *
   * This wrapper simply runs it on a worker thread so the
   * GUI thread is not blocked by mmdc.
   */

  using Result = QPair<QString, QString>;

  auto *watcher = new QFutureWatcher<Result>(this);

  QFuture<Result> future = QtConcurrent::run([this, mermaidSource]() -> Result {
    QString errorMessage;

    const QString svg = runMermaid(mermaidSource, errorMessage);

    return qMakePair(svg, errorMessage);
  });

  connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher]() {
    const Result result = watcher->result();

    watcher->deleteLater();

    const QString &svg = result.first;
    const QString &error = result.second;

    if (svg.isEmpty()) {
      emit renderFailed(
          error.isEmpty() ? QStringLiteral("Mermaid rendering failed") : error);
      return;
    }

    emit svgReady(svg);
  });

  watcher->setFuture(future);
}