#pragma once

#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QRectF>
#include <QtGui/QPainter>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <memory>

namespace lunasvg { class Document; }

class LunasvgRenderer : public QObject
{
  Q_OBJECT
public:
  explicit LunasvgRenderer(QObject *parent = nullptr);
  explicit LunasvgRenderer(const QString &filename, QObject *parent = nullptr);
  explicit LunasvgRenderer(const QByteArray &contents, QObject *parent = nullptr);
  ~LunasvgRenderer() override;

  bool load(const QString &filename);
  bool load(const QByteArray &contents);
  bool isValid() const;

  QSize defaultSize() const;
  QRectF viewBox() const;

  void render(QPainter *painter);
  void render(QPainter *painter, const QRectF &bounds);

  QImage toImage(const QSize &size = QSize());
  QPixmap toPixmap(const QSize &size = QSize());

private:
  std::unique_ptr<lunasvg::Document> m_document;
  QRectF m_viewBox;
  QSize m_intrinsicSize;
};