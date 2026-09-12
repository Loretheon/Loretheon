// DiagramDocument.h
#pragma once

#include <QObject>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QSvgRenderer>

class DiagramDocument : public QObject {
  Q_OBJECT
public:
  explicit DiagramDocument(QObject *parent = nullptr);

  void setSvg(const QString &themedSvg);
  void clear();

  bool isEmpty() const { return m_svg.isEmpty(); }
  QString svg() const { return m_svg; }
  QSize naturalSize() const { return m_naturalSize; }
  QSvgRenderer *renderer() { return &m_renderer; }

  QString idAt(const QPointF &svgPoint) const;
  QString nameForId(const QString &id) const;
  QRectF boundsForId(const QString &id) const;
  QStringList allIds() const;

  signals:
    void changed();

private:
  struct Region {
    QString id;
    QString name;
    QString kind;
    QRectF  bounds;
  };

  void rebuild();

  QString     m_svg;
  QSize       m_naturalSize;
  QSvgRenderer m_renderer;
  QVector<Region> m_regions;
};