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
  enum class NodeKind { Plain, Reference, External, Application };

  struct NodeInfo {
    QString id;
    QString name;
    QString kind;
    NodeKind nodeKind = NodeKind::Plain;
    QString referencePath;
    QRectF  bounds;
  };

  explicit DiagramDocument(QObject *parent = nullptr);

  void setSvg(const QString &themedSvg);
  void clear();

  bool isEmpty() const { return m_svg.isEmpty(); }
  QString svg() const { return m_svg; }
  QSize naturalSize() const { return m_naturalSize; }
  QSvgRenderer *renderer() { return &m_renderer; }

  QString idAt(const QPointF &svgPoint) const;
  QString nameForId(const QString &id) const;
  QRectF  boundsForId(const QString &id) const;
  NodeInfo infoForId(const QString &id) const;
  QStringList allIds() const;

  void setProjectRoot(const QString &root);
  QString projectRoot() const { return m_projectRoot; }

  static QString extractReference(const QString &label);
  static bool looksLikeReference(const QString &label);

  signals:
    void changed();

private:
  void rebuild();
  void classifyNode(NodeInfo &info, const QString &urlAttr);

  QString m_svg;
  QString m_projectRoot;
  QSize   m_naturalSize;
  QSvgRenderer m_renderer;
  QVector<NodeInfo> m_regions;
};