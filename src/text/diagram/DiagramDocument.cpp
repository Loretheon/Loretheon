#include "DiagramDocument.h"

#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

namespace {
const QRegularExpression kWikiLink(R"(^\s*\[\[([^\]]+)\]\]\s*$)");
const QStringList kRefExts = {"md", "markdown", "txt", "dot", "gv"};

QPointF wrapperTranslate(const QDomDocument &doc) {
  const QDomNodeList gs = doc.elementsByTagName("g");
  for (int i = 0; i < gs.count(); ++i) {
    QDomElement g = gs.at(i).toElement();
    if (g.isNull()) continue;
    if (g.attribute("class") == "graph") {
      const QString t = g.attribute("transform");
      static const QRegularExpression re(
          R"(translate\(\s*(-?[\d.]+)[\s,]+(-?[\d.]+)\s*\))");
      const auto m = re.match(t);
      if (m.hasMatch()) {
        return QPointF(m.captured(1).toDouble(), m.captured(2).toDouble());
      }
    }
  }
  return QPointF(0, 0);
}

QString urlFromNodeGroup(const QDomElement &g) {
  QDomNode n = g.parentNode();
  while (!n.isNull()) {
    QDomElement p = n.toElement();
    if (!p.isNull() && p.tagName() == "a") {
      const QString href = p.attribute("xlink:href");
      if (!href.isEmpty()) return href;
      const QString href2 = p.attribute("href");
      if (!href2.isEmpty()) return href2;
    }
    n = n.parentNode();
  }

  if (g.hasAttribute("url")) return g.attribute("url");
  if (g.hasAttribute("xlink:href")) return g.attribute("xlink:href");
  if (g.hasAttribute("href")) return g.attribute("href");

  const QDomNodeList anchors = g.elementsByTagName("a");
  for (int i = 0; i < anchors.count(); ++i) {
    QDomElement a = anchors.at(i).toElement();
    if (a.isNull()) continue;
    const QString href = a.attribute("xlink:href");
    if (!href.isEmpty()) return href;
    const QString href2 = a.attribute("href");
    if (!href2.isEmpty()) return href2;
  }

  return {};
}
}

DiagramDocument::DiagramDocument(QObject *parent) : QObject(parent) {}

void DiagramDocument::setProjectRoot(const QString &root) {
  m_projectRoot = root;
}

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

bool DiagramDocument::looksLikeReference(const QString &label) {
  if (label.isEmpty()) return false;
  if (kWikiLink.match(label).hasMatch()) return true;
  const QFileInfo fi(label);
  const QString ext = fi.suffix().toLower();
  return kRefExts.contains(ext);
}

QString DiagramDocument::extractReference(const QString &label) {
  const auto m = kWikiLink.match(label);
  if (m.hasMatch()) return m.captured(1).trimmed();
  return label.trimmed();
}

void DiagramDocument::classifyNode(NodeInfo &info, const QString &urlAttr) {
  info.nodeKind = NodeKind::Plain;
  info.referencePath.clear();

  if (info.kind != "node") return;

  if (!urlAttr.isEmpty()) {
    if (urlAttr.startsWith("app://", Qt::CaseInsensitive)) {
      info.nodeKind = NodeKind::Application;
      info.referencePath = urlAttr.mid(6);
      return;
    }

    const QUrl u(urlAttr, QUrl::StrictMode);
    const QString scheme = u.scheme().toLower();

    if (scheme == "http" || scheme == "https" ||
        scheme == "mailto" || scheme == "ftp") {
      info.nodeKind = NodeKind::External;
      info.referencePath = urlAttr;
      return;
        }
    if (scheme == "file") {
      info.nodeKind = NodeKind::Reference;
      info.referencePath = u.toLocalFile();
      return;
    }

    QString path = urlAttr;
    if (QFileInfo(path).isRelative() && !m_projectRoot.isEmpty()) {
      path = QDir(m_projectRoot).filePath(path);
    }
    info.nodeKind = NodeKind::Reference;
    info.referencePath = path;
    return;
  }

  if (!looksLikeReference(info.name)) return;

  const QString ref = extractReference(info.name);
  QString resolved = ref;
  if (QFileInfo(ref).isRelative() && !m_projectRoot.isEmpty()) {
    resolved = QDir(m_projectRoot).filePath(ref);
  }
  info.nodeKind = NodeKind::Reference;
  info.referencePath = resolved;
}
void DiagramDocument::rebuild() {
  m_regions.clear();
  m_naturalSize = m_renderer.defaultSize();

  if (m_svg.isEmpty()) return;

  QDomDocument doc;
  if (!doc.setContent(m_svg)) return;

  const QPointF offset = wrapperTranslate(doc);

  const QDomNodeList groups = doc.elementsByTagName("g");
  for (int i = 0; i < groups.count(); ++i) {
    QDomElement g = groups.at(i).toElement();
    if (g.isNull()) continue;

    const QString cls = g.attribute("class");
    if (cls != "node" && cls != "edge" && cls != "cluster") continue;

    QString id = g.attribute("id");
    if (id.isEmpty()) id = g.attribute("xml:id");
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
              double minX = nums[0], maxX = nums[0];
              double minY = nums[1], maxY = nums[1];
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
    bounds.translate(offset);

    NodeInfo info;
    info.id = id;
    info.name = name;
    info.kind = cls;
    info.bounds = bounds;
    classifyNode(info, urlFromNodeGroup(g));
    m_regions.append(info);
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
  for (const auto &r : m_regions)
    if (r.id == id) return r.name;
  return {};
}

QRectF DiagramDocument::boundsForId(const QString &id) const {
  for (const auto &r : m_regions)
    if (r.id == id) return r.bounds;
  return {};
}

DiagramDocument::NodeInfo DiagramDocument::infoForId(const QString &id) const {
  for (const auto &r : m_regions)
    if (r.id == id) return r;
  return {};
}

QStringList DiagramDocument::allIds() const {
  QStringList out;
  out.reserve(m_regions.size());
  for (const auto &r : m_regions) out.append(r.id);
  return out;
}