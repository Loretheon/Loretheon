// DiagramDocument.cpp
#include "DiagramDocument.h"

#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QRegularExpression>
#include <QDebug>

DiagramDocument::DiagramDocument(QObject *parent) : QObject(parent) {}

void DiagramDocument::setSvg(const QString &themedSvg) {
  m_svg = themedSvg;
  m_renderer.load(m_svg.toUtf8());
  rebuild();
  emit changed();
}

void DiagramDocument::clear() {
  m_svg.clear();
  m_renderer.load(QByteArray());
  m_regions.clear();
  m_naturalSize = QSize();
  emit changed();
}

void DiagramDocument::rebuild() {
  m_regions.clear();
  m_naturalSize = m_renderer.defaultSize();

  if (m_svg.isEmpty()) return;

  QDomDocument doc;
  if (!doc.setContent(m_svg)) return;

  const QDomNodeList groups = doc.elementsByTagName("g");
  for (int i = 0; i < groups.count(); ++i) {
    QDomElement g = groups.at(i).toElement();
    if (g.isNull()) continue;

    const QString cls = g.attribute("class");
    if (cls != "node" && cls != "edge" && cls != "cluster") continue;

    const QString id = g.attribute("id");
    if (id.isEmpty()) continue;

    QString name;
    const QDomNodeList titles = g.elementsByTagName("title");
    if (titles.count() > 0) {
      name = titles.at(0).toElement().text();
    }

    QRectF bounds;
    if (m_renderer.elementExists(id)) {
      bounds = m_renderer.boundsOnElement(id);
    }

    if (bounds.isNull()) {
      for (const QString &tag : {"polygon", "path", "ellipse", "rect"}) {
        const QDomNodeList kids = g.elementsByTagName(tag);
        for (int k = 0; k < kids.count(); ++k) {
          QDomElement el = kids.at(k).toElement();
          if (el.isNull()) continue;

          if (tag == "rect") {
            QRectF r(el.attribute("x").toDouble(),
                     el.attribute("y").toDouble(),
                     el.attribute("width").toDouble(),
                     el.attribute("height").toDouble());
            bounds = bounds.isNull() ? r : bounds.united(r);
          } else if (tag == "ellipse") {
            const double cx = el.attribute("cx").toDouble();
            const double cy = el.attribute("cy").toDouble();
            const double rx = el.attribute("rx").toDouble();
            const double ry = el.attribute("ry").toDouble();
            QRectF r(cx - rx, cy - ry, rx * 2, ry * 2);
            bounds = bounds.isNull() ? r : bounds.united(r);
          } else if (tag == "polygon") {
            const QString pts = el.attribute("points");
            const QStringList nums = pts.split(QRegularExpression("[\\s,]+"),
                                               Qt::SkipEmptyParts);
            if (nums.size() >= 2) {
              double minX = nums[0].toDouble();
              double maxX = minX;
              double minY = nums[1].toDouble();
              double maxY = minY;
              for (int n = 2; n + 1 < nums.size(); n += 2) {
                const double x = nums[n].toDouble();
                const double y = nums[n + 1].toDouble();
                minX = qMin(minX, x); maxX = qMax(maxX, x);
                minY = qMin(minY, y); maxY = qMax(maxY, y);
              }
              QRectF r(minX, minY, maxX - minX, maxY - minY);
              bounds = bounds.isNull() ? r : bounds.united(r);
            }
          } else if (tag == "path") {
            const QString d = el.attribute("d");
            const QRegularExpression re("-?\\d+(?:\\.\\d+)?");
            auto it = re.globalMatch(d);
            QVector<double> nums;
            while (it.hasNext()) nums.append(it.next().captured().toDouble());
            if (nums.size() >= 2) {
              double minX = nums[0];
              double maxX = nums[0];
              double minY = nums[1];
              double maxY = nums[1];
              for (int n = 2; n + 1 < nums.size(); n += 2) {
                minX = qMin(minX, nums[n]); maxX = qMax(maxX, nums[n]);
                minY = qMin(minY, nums[n + 1]); maxY = qMax(maxY, nums[n + 1]);
              }
              QRectF r(minX, minY, maxX - minX, maxY - minY);
              bounds = bounds.isNull() ? r : bounds.united(r);
            }
          }
        }
      }
    }

    if (bounds.isNull()) continue;

    Region region;
    region.id = id;
    region.name = name;
    region.kind = cls;
    region.bounds = bounds;
    m_regions.append(region);
  }
}

QString DiagramDocument::idAt(const QPointF &svgPoint) const {
  for (int i = m_regions.size() - 1; i >= 0; --i) {
    if (m_regions[i].kind != "node") continue;
    if (m_regions[i].bounds.contains(svgPoint)) return m_regions[i].id;
  }
  return {};
}

QString DiagramDocument::nameForId(const QString &id) const {
  for (const Region &r : m_regions)
    if (r.id == id) return r.name;
  return {};
}

QRectF DiagramDocument::boundsForId(const QString &id) const {
  for (const Region &r : m_regions)
    if (r.id == id) return r.bounds;
  return {};
}

QStringList DiagramDocument::allIds() const {
  QStringList out;
  out.reserve(m_regions.size());
  for (const Region &r : m_regions) out.append(r.id);
  return out;
}