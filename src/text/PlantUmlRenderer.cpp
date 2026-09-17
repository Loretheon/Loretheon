#include "../../include/text/PlantUmlRenderer.h"

#include "SvgThemer.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

PlantUmlRenderer::PlantUmlRenderer(QObject *parent) : QObject(parent) {}

PlantUmlRenderer::~PlantUmlRenderer() = default;

void PlantUmlRenderer::setThemeTokens(const ThemeTokens &tokens) {
  m_tokens = tokens;
}

bool PlantUmlRenderer::isPlantUmlAvailable() {
  const QStringList searchPaths = {
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
          "/plantuml/plantuml.jar",
      "/usr/share/plantuml/plantuml.jar",
      QDir::homePath() + "/.local/share/plantuml/plantuml.jar"};

  for (const auto &path : searchPaths) {
    if (QFileInfo(path).exists()) return true;
  }

  QProcess p;
  p.start("plantuml", {"-version"});
  return p.waitForFinished(2000) && p.exitCode() == 0;
}

QString PlantUmlRenderer::runPlantUml(const QString &source,
                                      QString &errorMessage) {
  errorMessage.clear();

  QStringList jarPaths = {
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
          "/plantuml/plantuml.jar",
      "/usr/share/plantuml/plantuml.jar",
      QDir::homePath() + "/.local/share/plantuml/plantuml.jar"};

  QString jarPath;
  for (const auto &path : jarPaths) {
    if (QFileInfo(path).exists()) {
      jarPath = path;
      break;
    }
  }

  QProcess p;
  if (!jarPath.isEmpty()) {
    p.start("java", {"-jar", jarPath, "-tsvg", "-pipe"});
  } else {
    p.start("plantuml", {"-tsvg", "-pipe"});
  }

  if (!p.waitForStarted(3000)) {
    errorMessage = "Failed to start PlantUML. Is it installed?";
    return {};
  }

  p.write(source.toUtf8());
  p.closeWriteChannel();

  if (!p.waitForFinished(15000)) {
    p.kill();
    p.waitForFinished(1000);
    errorMessage = "PlantUML rendering timed out";
    return {};
  }

  if (p.exitCode() != 0) {
    errorMessage = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (errorMessage.isEmpty())
      errorMessage = QString("PlantUML exited with code %1").arg(p.exitCode());
    return {};
  }

  return SvgThemer::applyTheme(QString::fromUtf8(p.readAllStandardOutput()),
                               m_tokens);
}

void PlantUmlRenderer::renderToSvgAsync(const QString &plantUmlSource) {
  auto *p = new QProcess(this);

  QStringList jarPaths = {
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
          "/plantuml/plantuml.jar",
      "/usr/share/plantuml/plantuml.jar",
      QDir::homePath() + "/.local/share/plantuml/plantuml.jar"};

  QString jarPath;
  for (const auto &path : jarPaths) {
    if (QFileInfo(path).exists()) {
      jarPath = path;
      break;
    }
  }

  if (!jarPath.isEmpty()) {
    p->setProgram("java");
    p->setArguments({"-jar", jarPath, "-tsvg", "-pipe"});
  } else {
    p->setProgram("plantuml");
    p->setArguments({"-tsvg", "-pipe"});
  }

  connect(p, &QProcess::errorOccurred, this,
          [this, p](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
              emit renderFailed("Failed to start PlantUML. Is it installed?");
              p->deleteLater();
            }
          });

  connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this, p](int code, QProcess::ExitStatus status) {
            const QByteArray out = p->readAllStandardOutput();
            const QByteArray err = p->readAllStandardError();
            p->deleteLater();
            if (status != QProcess::NormalExit || code != 0) {
              const QString errMsg = QString::fromUtf8(err).trimmed();
              emit renderFailed(errMsg.isEmpty()
                                    ? QString("PlantUML exited with code %1")
                                          .arg(code)
                                    : errMsg);
              return;
            }
            emit svgReady(
                SvgThemer::applyTheme(QString::fromUtf8(out), m_tokens));
          });

  p->start();
  if (!p->waitForStarted(3000)) {
    p->deleteLater();
    emit renderFailed("Failed to start PlantUML. Is it installed?");
    return;
  }
  p->write(plantUmlSource.toUtf8());
  p->closeWriteChannel();
}