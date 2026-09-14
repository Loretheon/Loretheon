#ifndef PLANTUMLRENDERER_H
#define PLANTUMLRENDERER_H

#include <QObject>
#include <QString>

#include "ThemeAware.h"

class PlantUmlRenderer : public QObject, public ThemeAware {
  Q_OBJECT

public:
  explicit PlantUmlRenderer(QObject *parent = nullptr);
  ~PlantUmlRenderer();

  void renderToSvgAsync(const QString &plantUmlSource);
  static bool isPlantUmlAvailable();

  void setThemeTokens(const ThemeTokens &tokens) override;

  signals:
    void svgReady(const QString &svg);
  void renderFailed(const QString &errorMessage);

private:
  QString runPlantUml(const QString &source, QString &errorMessage);

  ThemeTokens m_tokens;
};

#endif // PLANTUMLRENDERER_H