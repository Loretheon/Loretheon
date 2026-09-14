#include "SvgThemer.h"

#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>

namespace {
QString toHex(const QColor &c) {
  return c.name(QColor::HexRgb);
}

bool isColorProperty(const QString &key) {
  return key == "fill" || key == "stroke" || key == "stop-color";
}

bool hasClass(const QDomElement &el, const QString &name) {
  const QStringList parts =
      el.attribute("class").split(' ', Qt::SkipEmptyParts);
  return parts.contains(name);
}

bool ancestorHasClass(const QDomElement &el, const QStringList &names) {
  QDomNode n = el.parentNode();
  while (!n.isNull()) {
    const QDomElement p = n.toElement();
    if (!p.isNull()) {
      const QStringList parts =
          p.attribute("class").split(' ', Qt::SkipEmptyParts);
      for (const QString &name : names) {
        if (parts.contains(name)) return true;
      }
    }
    n = n.parentNode();
  }
  return false;
}

QDomElement nearestLabelGroup(QDomElement el) {
  QDomNode n = el.parentNode();
  while (!n.isNull()) {
    const QDomElement p = n.toElement();
    if (!p.isNull() && hasClass(p, "label")) return p;
    n = n.parentNode();
  }
  return {};
}

bool inMarker(const QDomElement &el) {
  QDomNode n = el.parentNode();
  while (!n.isNull()) {
    const QDomElement p = n.toElement();
    if (!p.isNull() && p.tagName() == "marker") return true;
    n = n.parentNode();
  }
  return false;
}

void stripColorAttributes(QDomElement &el) {
  static const QStringList attrs = {"fill", "stroke", "stop-color"};
  for (const QString &a : attrs) el.removeAttribute(a);

  if (el.hasAttribute("style")) {
    QString s = el.attribute("style");
    s.remove(QRegularExpression(
        R"(\b(fill|stroke|stop-color)\s*:\s*[^;]+;?)",
        QRegularExpression::CaseInsensitiveOption));
    s = s.trimmed();
    if (s.isEmpty()) el.removeAttribute("style");
    else el.setAttribute("style", s);
  }
}

QHash<QString, QHash<QString, QString>> parseCssRules(QDomDocument &doc) {
  QHash<QString, QHash<QString, QString>> rules;

  const QDomNodeList styles = doc.elementsByTagName("style");
  if (styles.isEmpty()) return rules;

  static const QRegularExpression ruleRe(R"(([^{}]+)\{([^{}]*)\})");
  static const QRegularExpression declRe(R"(([a-zA-Z-]+)\s*:\s*([^;]+);?)");

  for (int s = 0; s < styles.count(); ++s) {
    const QDomElement styleEl = styles.at(s).toElement();
    if (styleEl.isNull()) continue;
    const QString css = styleEl.text();

    auto it = ruleRe.globalMatch(css);
    while (it.hasNext()) {
      const auto m = it.next();
      const QString selectorList = m.captured(1).trimmed();
      const QString body = m.captured(2);

      QHash<QString, QString> decls;
      auto dit = declRe.globalMatch(body);
      while (dit.hasNext()) {
        const auto d = dit.next();
        decls.insert(d.captured(1).trimmed().toLower(),
                     d.captured(2).trimmed());
      }

      const QStringList selectors =
          selectorList.split(',', Qt::SkipEmptyParts);
      for (const QString &raw : selectors) {
        rules.insert(raw.trimmed(), decls);
      }
    }
  }

  return rules;
}

QStringList selectorsFor(const QDomElement &el) {
  QStringList out;

  QString pathPrefix;
  QDomElement cur = el;

  while (!cur.isNull() && cur.tagName() != "svg") {
    const QString tag = cur.tagName();
    const QString cls = cur.attribute("class");
    const QString id = cur.attribute("id");

    QStringList localSelectors;

    if (!cls.isEmpty()) {
      for (const QString &c : cls.split(' ', Qt::SkipEmptyParts)) {
        localSelectors << QStringLiteral("%1.%2").arg(tag, c);
      }
    }

    localSelectors << tag;

    for (const QString &local : localSelectors) {
      out << local;
      if (!pathPrefix.isEmpty()) {
        out << pathPrefix + " " + local;
      }
    }

    QString stepPrefix;
    if (!cls.isEmpty()) {
      const QStringList classes = cls.split(' ', Qt::SkipEmptyParts);
      if (!classes.isEmpty()) {
        stepPrefix = QStringLiteral("%1.%2").arg(tag, classes.first());
      }
    }
    if (stepPrefix.isEmpty()) stepPrefix = tag;

    pathPrefix = pathPrefix.isEmpty() ? stepPrefix
                                      : stepPrefix + " " + pathPrefix;

    if (!id.isEmpty()) {
      out << QStringLiteral("#%1").arg(id);
      out << QStringLiteral("#%1 %2").arg(id, tag);
    }

    cur = cur.parentNode().toElement();
  }

  return out;
}

void inlineNonColorStyles(
    QDomDocument &doc,
    const QHash<QString, QHash<QString, QString>> &rules) {
  if (rules.isEmpty()) return;

  const QStringList tags = {"text",     "tspan", "rect",    "polygon",
                            "ellipse",  "path",  "circle",  "line",
                            "polyline", "g",     "marker"};

  for (const QString &tag : tags) {
    const QDomNodeList elems = doc.elementsByTagName(tag);
    for (int i = 0; i < elems.count(); ++i) {
      QDomElement el = elems.at(i).toElement();
      if (el.isNull()) continue;

      QHash<QString, QString> merged;
      const QStringList selectors = selectorsFor(el);

      for (const QString &sel : selectors) {
        auto it = rules.constFind(sel);
        if (it == rules.constEnd()) continue;
        for (auto d = it->constBegin(); d != it->constEnd(); ++d) {
          merged.insert(d.key(), d.value());
        }
      }

      if (merged.isEmpty()) continue;

      for (auto d = merged.constBegin(); d != merged.constEnd(); ++d) {
        if (isColorProperty(d.key())) continue;
        if (el.hasAttribute(d.key())) continue;
        el.setAttribute(d.key(), d.value());
      }
    }
  }
}

void stripStyleBlocks(QDomDocument &doc) {
  const QDomNodeList styles = doc.elementsByTagName("style");
  for (int i = styles.count() - 1; i >= 0; --i) {
    QDomNode n = styles.at(i);
    if (!n.isNull() && !n.parentNode().isNull()) {
      n.parentNode().removeChild(n);
    }
  }
}

void flattenNestedTspans(QDomDocument &doc) {
  const QDomNodeList tspans = doc.elementsByTagName("tspan");
  for (int i = 0; i < tspans.count(); ++i) {
    QDomElement el = tspans.at(i).toElement();
    if (el.isNull()) continue;

    QString combined;
    bool allChildrenAreTspans = true;
    bool hasChildElement = false;

    for (QDomNode c = el.firstChild(); !c.isNull(); c = c.nextSibling()) {
      if (c.isText()) {
        combined += c.toText().data();
      } else if (c.isElement()) {
        QDomElement ce = c.toElement();
        if (ce.tagName() != "tspan") {
          allChildrenAreTspans = false;
          break;
        }
        hasChildElement = true;
        combined += ce.text();
      }
    }

    if (!allChildrenAreTspans || !hasChildElement) continue;

    while (!el.firstChild().isNull()) {
      el.removeChild(el.firstChild());
    }
    el.appendChild(doc.createTextNode(combined));
  }
}

double parseFontSizePx(const QString &value) {
  if (value.isEmpty()) return 0.0;

  static const QRegularExpression re(
      R"(^\s*([0-9]*\.?[0-9]+)\s*(px|pt|em|rem|%)?\s*$)",
      QRegularExpression::CaseInsensitiveOption);
  const auto m = re.match(value);
  if (!m.hasMatch()) return 0.0;

  bool ok = false;
  const double num = m.captured(1).toDouble(&ok);
  if (!ok || num <= 0.0) return 0.0;

  const QString unit = m.captured(2).toLower();
  if (unit.isEmpty() || unit == "px") return num;
  if (unit == "pt") return num * (96.0 / 72.0);
  if (unit == "em" || unit == "rem") return num * 14.0;
  if (unit == "%") return num * 14.0 / 100.0;
  return num;
}

double fontSizeFor(const QDomElement &textEl) {
  constexpr double kDefault = 14.0;

  // 1) explicit attribute
  if (textEl.hasAttribute("font-size")) {
    const double v = parseFontSizePx(textEl.attribute("font-size"));
    if (v > 0.0) return v;
  }

  // 2) inline style
  if (textEl.hasAttribute("style")) {
    static const QRegularExpression styleRe(
        R"((?:^|;)\s*font-size\s*:\s*([^;]+))",
        QRegularExpression::CaseInsensitiveOption);
    const auto m = styleRe.match(textEl.attribute("style"));
    if (m.hasMatch()) {
      const double v = parseFontSizePx(m.captured(1).trimmed());
      if (v > 0.0) return v;
    }
  }

  // 3) inherited from ancestor
  QDomNode n = textEl.parentNode();
  while (!n.isNull()) {
    const QDomElement p = n.toElement();
    if (!p.isNull()) {
      if (p.hasAttribute("font-size")) {
        const double v = parseFontSizePx(p.attribute("font-size"));
        if (v > 0.0) return v;
      }
      if (p.hasAttribute("style")) {
        static const QRegularExpression styleRe(
            R"((?:^|;)\s*font-size\s*:\s*([^;]+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = styleRe.match(p.attribute("style"));
        if (m.hasMatch()) {
          const double v = parseFontSizePx(m.captured(1).trimmed());
          if (v > 0.0) return v;
        }
      }
    }
    n = n.parentNode();
  }

  return kDefault;
}

// Applies a translate(dx, dy) while preserving any existing translate.
// Returns true on success. Non-translate components (scale/rotate/matrix)
// cause the original transform to be kept untouched.
bool applyTranslatePreserving(
    QDomElement &el, const QString &tx, const QString &ty) {
  const QString existing = el.attribute("transform").trimmed();
  if (existing.isEmpty()) {
    el.setAttribute(
        "transform",
        QStringLiteral("translate(%1,%2)").arg(tx, ty));
    return true;
  }

  static const QRegularExpression translateRe(
      R"(^\s*translate\(\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"""
      R"(\s*[,\s]\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)?\s*\)\s*$)",
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);

  const auto m = translateRe.match(existing);
  if (m.hasMatch()) {
    // Replace existing translate with the new one (we want to recenter,
    // not accumulate). This is the safest behavior for a theming pass.
    el.setAttribute(
        "transform",
        QStringLiteral("translate(%1,%2)").arg(tx, ty));
    return true;
  }

  // Non-translate transform; leave it alone.
  return false;
}

void recenterTextVertically(QDomDocument &doc) {
  const double kLineHeightFactor = 1.1;
  // Approximate (ascent - descent) / 2 as a fraction of font size so that
  // the visual center of a line of text sits at the requested y.
  const double kBaselineComp = 0.35;

  const QDomNodeList texts = doc.elementsByTagName("text");
  for (int t = 0; t < texts.count(); ++t) {
    QDomElement textEl = texts.at(t).toElement();
    if (textEl.isNull()) continue;

    const bool isEdgeLabel = ancestorHasClass(textEl, {"edgeLabel"});
    const bool isNodeLabel = ancestorHasClass(
        textEl, {"node", "label", "rough-node", "image-shape",
                 "icon-shape", "cluster", "cluster-label"});

    if (!isEdgeLabel && !isNodeLabel) continue;

    const double fontSize = fontSizeFor(textEl);
    const double lineHeight = kLineHeightFactor * fontSize;
    const double baselineComp = kBaselineComp * fontSize;

    QDomElement labelGroup = nearestLabelGroup(textEl);

    // Only consider tspans that are direct children of this <text> element
    // to avoid grabbing nested tspans belonging to other text blocks.
    QList<QDomElement> rows;
    for (QDomNode c = textEl.firstChild(); !c.isNull(); c = c.nextSibling()) {
      const QDomElement sp = c.toElement();
      if (sp.isNull() || sp.tagName() != "tspan") continue;
      if (hasClass(sp, "row") || sp.hasAttribute("y") ||
          sp.hasAttribute("dy")) {
        bool dup = false;
        for (const auto &r : rows) {
          if (r == sp) { dup = true; break; }
        }
        if (!dup) rows.append(sp);
      }
    }

    if (rows.isEmpty()) {
      textEl.setAttribute("y", QString::number(baselineComp, 'f', 3));
      textEl.removeAttribute("dy");
      textEl.setAttribute("text-anchor", "middle");
      textEl.setAttribute("dominant-baseline", "central");
      if (!labelGroup.isNull()) {
        // Preserve existing translate, don't clobber.
        // No-op here: label group keeps its current positioning.
        Q_UNUSED(labelGroup);
      }
      continue;
    }

    const int n = rows.size();
    const double centerOffset = (n - 1) / 2.0;

    for (int i = 0; i < n; ++i) {
      const double yUser = (i - centerOffset) * lineHeight + baselineComp;

      QDomElement row = rows[i];
      row.removeAttribute("dy");
      row.setAttribute("y", QString::number(yUser, 'f', 3));
      row.setAttribute("x", "0");
      row.setAttribute("text-anchor", "middle");
    }

    textEl.removeAttribute("y");
    textEl.removeAttribute("dy");
    textEl.setAttribute("text-anchor", "middle");
    textEl.setAttribute("dominant-baseline", "central");

    // Do NOT reset labelGroup's transform to translate(0,0); that
    // destroys the layout-provided anchor position.
    Q_UNUSED(labelGroup);
  }
}
} // namespace

QString SvgThemer::applyTheme(const QString &svg, const ThemeTokens &tokens) {
  if (svg.isEmpty()) return svg;

  QDomDocument doc;
  if (!doc.setContent(svg)) return svg;

  QDomElement root = doc.documentElement();
  if (root.isNull() || root.tagName() != "svg") return svg;

  const auto cssRules = parseCssRules(doc);
  inlineNonColorStyles(doc, cssRules);
  stripStyleBlocks(doc);
  flattenNestedTspans(doc);
  recenterTextVertically(doc);

  const QString nodeFill = toHex(tokens.nodeFill());
  const QString nodeStroke = toHex(tokens.nodeStroke());
  const QString clusterBkg = toHex(tokens.clusterBkg());
  const QString clusterBorder = toHex(tokens.clusterBorder());
  const QString edgeStroke = toHex(tokens.edgeStroke());
  const QString edgeLabelBkg = toHex(tokens.edgeLabelBackground());
  const QString markerFill = toHex(tokens.markerFill());
  const QString textFill = toHex(tokens.textFill());
  const QString titleFill = toHex(tokens.titleFill());

  const QStringList tags = {"g",       "text",   "tspan", "polygon",
                            "ellipse", "path",   "rect",  "circle",
                            "line",    "polyline", "use", "title",
                            "a",       "marker"};

  for (const QString &tag : tags) {
    const QDomNodeList elems = doc.elementsByTagName(tag);
    for (int i = 0; i < elems.count(); ++i) {
      QDomElement el = elems.at(i).toElement();
      if (el.isNull()) continue;
      stripColorAttributes(el);
    }
  }

  const QDomNodeList rectNodes = doc.elementsByTagName("rect");
  for (int i = 0; i < rectNodes.count(); ++i) {
    QDomElement el = rectNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (ancestorHasClass(el, {"cluster"})) {
      el.setAttribute("fill", clusterBkg);
      el.setAttribute("stroke", clusterBorder);
    } else if (ancestorHasClass(el, {"edgeLabel"})) {
      el.setAttribute("fill", edgeLabelBkg);
      el.setAttribute("stroke", edgeStroke);
    } else {
      el.setAttribute("fill", nodeFill);
      el.setAttribute("stroke", nodeStroke);
    }
  }

  const QDomNodeList polyNodes = doc.elementsByTagName("polygon");
  for (int i = 0; i < polyNodes.count(); ++i) {
    QDomElement el = polyNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (inMarker(el)) {
      el.setAttribute("fill", markerFill);
      el.setAttribute("stroke", markerFill);
    } else {
      el.setAttribute("fill", nodeFill);
      el.setAttribute("stroke", nodeStroke);
    }
  }

  const QDomNodeList ellipseNodes = doc.elementsByTagName("ellipse");
  for (int i = 0; i < ellipseNodes.count(); ++i) {
    QDomElement el = ellipseNodes.at(i).toElement();
    if (el.isNull()) continue;
    el.setAttribute("fill", nodeFill);
    el.setAttribute("stroke", nodeStroke);
  }

  const QDomNodeList circleNodes = doc.elementsByTagName("circle");
  for (int i = 0; i < circleNodes.count(); ++i) {
    QDomElement el = circleNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (inMarker(el)) {
      el.setAttribute("fill", markerFill);
      el.setAttribute("stroke", markerFill);
    } else {
      el.setAttribute("fill", nodeFill);
      el.setAttribute("stroke", nodeStroke);
    }
  }

  const QDomNodeList pathNodes = doc.elementsByTagName("path");
  for (int i = 0; i < pathNodes.count(); ++i) {
    QDomElement el = pathNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (inMarker(el)) {
      el.setAttribute("fill", markerFill);
      el.setAttribute("stroke", markerFill);
      continue;
    }
    el.setAttribute("stroke", edgeStroke);
    if (ancestorHasClass(el, {"cluster"})) {
      el.setAttribute("fill", clusterBkg);
    } else if (ancestorHasClass(el, {"node"})) {
      el.setAttribute("fill", nodeFill);
    } else {
      el.setAttribute("fill", "none");
    }
  }

  const QDomNodeList lineNodes = doc.elementsByTagName("line");
  for (int i = 0; i < lineNodes.count(); ++i) {
    QDomElement el = lineNodes.at(i).toElement();
    if (el.isNull()) continue;
    el.setAttribute("stroke", edgeStroke);
  }

  const QDomNodeList polylineNodes = doc.elementsByTagName("polyline");
  for (int i = 0; i < polylineNodes.count(); ++i) {
    QDomElement el = polylineNodes.at(i).toElement();
    if (el.isNull()) continue;
    el.setAttribute("stroke", edgeStroke);
    el.setAttribute("fill", "none");
  }

  const QDomNodeList textNodes = doc.elementsByTagName("text");
  for (int i = 0; i < textNodes.count(); ++i) {
    QDomElement el = textNodes.at(i).toElement();
    if (el.isNull()) continue;
    if (ancestorHasClass(el, {"cluster", "cluster-label"})) {
      el.setAttribute("fill", titleFill);
    } else {
      el.setAttribute("fill", textFill);
    }
  }

  const QDomNodeList tspanNodes = doc.elementsByTagName("tspan");
  for (int i = 0; i < tspanNodes.count(); ++i) {
    QDomElement el = tspanNodes.at(i).toElement();
    if (el.isNull()) continue;
    el.setAttribute("fill", textFill);
  }

  const QDomNodeList titleNodes = doc.elementsByTagName("title");
  for (int i = 0; i < titleNodes.count(); ++i) {
    QDomElement el = titleNodes.at(i).toElement();
    if (el.isNull()) continue;
    el.setAttribute("fill", titleFill);
  }

  root.removeAttribute("width");
  root.removeAttribute("height");
  root.removeAttribute("style");
  root.setAttribute("preserveAspectRatio", "xMidYMid meet");

  return doc.toString(-1);
}