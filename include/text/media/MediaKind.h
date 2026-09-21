#ifndef EPISTEME_MEDIAKIND_H
#define EPISTEME_MEDIAKIND_H

#include <QString>
#include <QStringList>

// The category of media a file falls into. Determines which widget in
// MediaPane is shown.
enum class MediaKind {
  None,
  Raster,   // png, jpg, gif, bmp, webp, tiff, ico
  Vector,   // svg
  Audio,    // mp3, wav, flac, ogg, m4a
  Video,    // mp4, webm, mkv, mov, avi
};

namespace MediaKinds {

inline QStringList rasterExtensions() {
  return {QStringLiteral("png"),  QStringLiteral("jpg"),
          QStringLiteral("jpeg"), QStringLiteral("bmp"),
          QStringLiteral("gif"),  QStringLiteral("webp"),
          QStringLiteral("tiff"), QStringLiteral("tif"),
          QStringLiteral("ico"),  QStringLiteral("ppm"),
          QStringLiteral("pgm"),  QStringLiteral("pbm")};
}

inline QStringList vectorExtensions() {
  return {QStringLiteral("svg")};
}

inline QStringList audioExtensions() {
  return {QStringLiteral("mp3"),  QStringLiteral("wav"),
          QStringLiteral("flac"), QStringLiteral("ogg"),
          QStringLiteral("m4a"),  QStringLiteral("aac"),
          QStringLiteral("opus")};
}

inline QStringList videoExtensions() {
  return {QStringLiteral("mp4"),  QStringLiteral("webm"),
          QStringLiteral("mkv"),  QStringLiteral("mov"),
          QStringLiteral("avi"),  QStringLiteral("m4v")};
}

inline MediaKind kindForExtension(const QString &extension) {
  QString key = extension.toLower();
  if (key.startsWith(QLatin1Char('.'))) {
    key.remove(0, 1);
  }
  if (key.isEmpty()) {
    return MediaKind::None;
  }

  if (rasterExtensions().contains(key)) return MediaKind::Raster;
  if (vectorExtensions().contains(key)) return MediaKind::Vector;
  if (audioExtensions().contains(key)) return MediaKind::Audio;
  if (videoExtensions().contains(key)) return MediaKind::Video;
  return MediaKind::None;
}

inline MediaKind kindForPath(const QString &path) {
  const int dot = path.lastIndexOf(QLatin1Char('.'));
  if (dot < 0) return MediaKind::None;
  return kindForExtension(path.mid(dot + 1));
}

inline bool isMediaPath(const QString &path) {
  return kindForPath(path) != MediaKind::None;
}

} // namespace MediaKinds

#endif // EPISTEME_MEDIAKIND_H