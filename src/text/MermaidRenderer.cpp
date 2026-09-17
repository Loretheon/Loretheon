#include "../../include/text/MermaidRenderer.h"

#include "SvgThemer.h"

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

  if (!stderrText.isEmpty()) return stderrText;

  const QString stdoutText =
      QString::fromUtf8(process->readAllStandardOutput()).trimmed();

  if (!stdoutText.isEmpty()) return stdoutText;

  return QStringLiteral("Mermaid exited with code %1").arg(process->exitCode());
}

void configureMermaidEnvironment(QProcess &process) {
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

  const QFileInfo mmdcInfo(QStringLiteral(EPISTEME_MERMAID_CLI));
  const QString mmdcDirectory = mmdcInfo.absolutePath();

  QString path = environment.value(QStringLiteral("PATH"));
  const QStringList pathEntries =
      path.split(QDir::listSeparator(), Qt::SkipEmptyParts);

  if (!mmdcDirectory.isEmpty() && !pathEntries.contains(mmdcDirectory)) {
    if (path.isEmpty()) path = mmdcDirectory;
    else path = mmdcDirectory + QDir::listSeparator() + path;
  }

  environment.insert(QStringLiteral("PATH"), path);
  environment.insert(QStringLiteral("PUPPETEER_DISABLE_HEADLESS_WARNING"),
                     QStringLiteral("true"));
  environment.insert(QStringLiteral("PUPPETEER_SKIP_CHROMIUM_DOWNLOAD"),
                     QStringLiteral("true"));

  process.setProcessEnvironment(environment);
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

void MermaidRenderer::setThemeTokens(const ThemeTokens &tokens) {
  m_tokens = tokens;
}

QString MermaidRenderer::writeMermaidConfig(const QString &dirPath) const {
  const QString path =
      QDir(dirPath).filePath(QStringLiteral("mermaid-config.json"));

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};

  const QString config = QStringLiteral(
      "{\n"
      "  \"htmlLabels\": false,\n"
      "  \"useMaxWidth\": false,\n"
      "  \"theme\": \"base\",\n"
      "  \"themeVariables\": {\n"
      "    \"background\": \"%1\",\n"
      "    \"primaryColor\": \"%2\",\n"
      "    \"primaryTextColor\": \"%3\",\n"
      "    \"primaryBorderColor\": \"%4\",\n"
      "    \"lineColor\": \"%5\",\n"
      "    \"secondaryColor\": \"%6\",\n"
      "    \"tertiaryColor\": \"%7\",\n"
      "    \"clusterBkg\": \"%8\",\n"
      "    \"clusterBorder\": \"%9\",\n"
      "    \"edgeLabelBackground\": \"%10\",\n"
      "    \"nodeBorder\": \"%4\",\n"
      "    \"nodeTextColor\": \"%3\",\n"
      "    \"titleColor\": \"%3\",\n"
      "    \"fontFamily\": \"sans-serif\",\n"
      "    \"fontSize\": \"14px\"\n"
      "  },\n"
      "  \"flowchart\": {\n"
      "    \"useMaxWidth\": false,\n"
      "    \"curve\": \"linear\"\n"
      "  }\n"
      "}\n")
      .arg(m_tokens.background().name(QColor::HexRgb),
           m_tokens.nodeFill().name(QColor::HexRgb),
           m_tokens.textFill().name(QColor::HexRgb),
           m_tokens.nodeStroke().name(QColor::HexRgb),
           m_tokens.edgeStroke().name(QColor::HexRgb),
           m_tokens.surface1.name(QColor::HexRgb),
           m_tokens.crust.name(QColor::HexRgb),
           m_tokens.clusterBkg().name(QColor::HexRgb),
           m_tokens.clusterBorder().name(QColor::HexRgb),
           m_tokens.edgeLabelBackground().name(QColor::HexRgb))
      .toUtf8();

  if (file.write(config.toUtf8()) != config.toUtf8().size()) return {};
  if (!file.flush()) return {};

  file.close();
  return path;
}

bool MermaidRenderer::isMermaidAvailable() {
  QProcess process;
  if (!startMermaid(process, {QStringLiteral("--version")}, 5000)) return false;
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

  if (!process.waitForFinished(30000)) {
    process.kill();
    process.waitForFinished(1000);
    errorMessage = QStringLiteral("Mermaid rendering timed out");
    return {};
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    errorMessage = mermaidErrorMessage(&process);
    return {};
  }

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

  return SvgThemer::applyTheme(svg, m_tokens);
}

void MermaidRenderer::renderToSvgAsync(const QString &mermaidSource) {
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