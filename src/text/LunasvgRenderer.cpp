#include "LunasvgRenderer.h"

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtGui/QPainter>
#include <lunasvg.h>

namespace {

bool parseViewBox(const QByteArray &data, QRectF &out) {
  static const QRegularExpression re(
      R"(viewBox\s*=\s*"([^"]+)\")",
      QRegularExpression::CaseInsensitiveOption);
  const QString svg = QString::fromUtf8(data.left(8192));
  const auto m = re.match(svg);
  if (!m.hasMatch()) {
    return false;
  }

  const QStringList parts = m.captured(1)
                                .split(QRegularExpression("[\\s,]+"),
                                       Qt::SkipEmptyParts);
  if (parts.size() != 4) {
    return false;
  }

  bool ok = true;
  const qreal x = parts[0].toDouble(&ok); if (!ok) { return false; }
  const qreal y = parts[1].toDouble(&ok); if (!ok) { return false; }
  const qreal w = parts[2].toDouble(&ok); if (!ok) { return false; }
  const qreal h = parts[3].toDouble(&ok); if (!ok) { return false; }
  if (w <= 0.0 || h <= 0.0) {
    return false;
  }

  out = QRectF(x, y, w, h);
  return true;
}

bool parseWidthHeight(const QByteArray &data, QSizeF &out) {
  static const QRegularExpression wRe(
      R"(<svg[^>]*\bwidth\s*=\s*"([^"]+)\")",
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression hRe(
      R"(<svg[^>]*\bheight\s*=\s*"([^"]+)\")",
      QRegularExpression::CaseInsensitiveOption);

  const QString svg = QString::fromUtf8(data.left(8192));

  auto parseLen = [](const QString &raw, qreal &value) -> bool {
    static const QRegularExpression numRe(
        R"(^\s*(-?[\d.]+)\s*(px|pt|pc|mm|cm|in|%)?\s*$)");
    const auto m = numRe.match(raw);
    if (!m.hasMatch()) return false;
    if (!m.captured(2).isEmpty() && m.captured(2) != "px") return false;
    bool ok = false;
    value = m.captured(1).toDouble(&ok);
    return ok;
  };

  const auto wm = wRe.match(svg);
  const auto hm = hRe.match(svg);
  if (!wm.hasMatch() || !hm.hasMatch()) {
    return false;
  }

  qreal w = 0.0;
  qreal h = 0.0;
  if (!parseLen(wm.captured(1), w)) { return false; }
  if (!parseLen(hm.captured(1), h)) { return false; }
  if (w <= 0.0 || h <= 0.0) { return false; }

  out = QSizeF(w, h);
  return true;
}

} // namespace

LunasvgRenderer::LunasvgRenderer(QObject *parent)
    : QObject(parent) {
}

LunasvgRenderer::LunasvgRenderer(const QString &filename, QObject *parent)
    : QObject(parent) {
  load(filename);
}

LunasvgRenderer::LunasvgRenderer(const QByteArray &contents, QObject *parent)
    : QObject(parent) {
  load(contents);
}

LunasvgRenderer::~LunasvgRenderer() = default;

bool LunasvgRenderer::load(const QString &filename) {
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QByteArray data = file.readAll();
  return load(data);
}

bool LunasvgRenderer::load(const QByteArray &contents) {
  m_document.reset();
  m_viewBox = QRectF();
  m_intrinsicSize = QSize();

  if (contents.isEmpty()) {
    return false;
  }

  m_document = lunasvg::Document::loadFromData(
      std::string(contents.constData(), contents.size()));
  if (!m_document) {
    return false;
  }

  QRectF vb;
  if (parseViewBox(contents, vb)) {
    m_viewBox = vb;
  } else {
    QSizeF wh;
    if (parseWidthHeight(contents, wh)) {
      m_viewBox = QRectF(0.0, 0.0, wh.width(), wh.height());
    } else {
      m_viewBox = QRectF(0.0, 0.0,
                         static_cast<qreal>(m_document->width()),
                         static_cast<qreal>(m_document->height()));
    }
  }

  m_intrinsicSize = QSize(qMax(1, qRound(m_viewBox.width())),
                          qMax(1, qRound(m_viewBox.height())));

  return true;
}

bool LunasvgRenderer::isValid() const {
  return m_document != nullptr;
}

QSize LunasvgRenderer::defaultSize() const {
  return m_intrinsicSize;
}

QRectF LunasvgRenderer::viewBox() const {
  return m_viewBox;
}

void LunasvgRenderer::render(QPainter *painter) {
  if (!m_document || !painter) {
    return;
  }
  render(painter, QRectF(QPointF(0, 0), QSizeF(m_intrinsicSize)));
}

void LunasvgRenderer::render(QPainter *painter, const QRectF &bounds) {
  if (!m_document || !painter) {
    return;
  }
  if (bounds.isEmpty()) {
    return;
  }
  if (m_viewBox.width() <= 0.0 || m_viewBox.height() <= 0.0) {
    return;
  }

  const qreal scale = qMin(bounds.width()  / m_viewBox.width(),
                           bounds.height() / m_viewBox.height());
  if (scale <= 0.0) {
    return;
  }

  const int w = qMax(1, qRound(m_viewBox.width()  * scale));
  const int h = qMax(1, qRound(m_viewBox.height() * scale));

  auto bitmap = m_document->renderToBitmap(w, h);
  if (bitmap.isNull()) {
    return;
  }
  bitmap.convertToRGBA();

  QImage rendered(bitmap.data(),
                  static_cast<int>(bitmap.width()),
                  static_cast<int>(bitmap.height()),
                  static_cast<int>(bitmap.stride()),
                  QImage::Format_RGBA8888);

  const QRectF target(bounds.left() + (bounds.width()  - w) * 0.5,
                      bounds.top()  + (bounds.height() - h) * 0.5,
                      w, h);

  painter->drawImage(target, rendered);
}

QImage LunasvgRenderer::toImage(const QSize &size) {
  if (!m_document) {
    return QImage();
  }
  QSize s = size.isValid() ? size : m_intrinsicSize;
  if (s.isEmpty()) {
    return QImage();
  }

  QImage image(s, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  render(&painter, QRectF(QPointF(0, 0), QSizeF(s)));
  return image;
}

QPixmap LunasvgRenderer::toPixmap(const QSize &size) {
  return QPixmap::fromImage(toImage(size));
}