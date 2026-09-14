#ifndef PLANTUML_RENDERER_H
#define PLANTUML_RENDERER_H

#include <QObject>
#include <QString>

class QProcess;

class PlantUmlRenderer : public QObject {
  Q_OBJECT

public:
  explicit PlantUmlRenderer(QObject *parent = nullptr);
  ~PlantUmlRenderer();

  void renderToSvgAsync(const QString &plantUmlSource);
  static bool isPlantUmlAvailable();

  signals:
    void svgReady(const QString &svg);
  void renderFailed(const QString &errorMessage);

private:
  QString runPlantUml(const QString &source, QString &errorMessage);
};

#endif