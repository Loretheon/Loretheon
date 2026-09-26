#ifndef EPISTEME_MEDIASETTINGS_H
#define EPISTEME_MEDIASETTINGS_H

#include "MediaKind.h"

#include <QColor>
#include <QString>

// Persisted viewer settings, stored in QSettings under "media/…".
// One instance reads and writes the whole group; per-file overrides are
// not stored, only per-kind defaults.
class MediaSettings {
public:
  static MediaSettings &instance();

  // Image fit mode.
  enum class FitMode {
    FitToWindow,
    ActualSize,
    CustomZoom,
  };

  FitMode imageFitMode() const;
  void setImageFitMode(FitMode mode);

  double imageZoom() const;
  void setImageZoom(double zoom);

  bool lockAspectRatio() const;
  void setLockAspectRatio(bool locked);

  // Whether an animated GIF should autoplay on open.
  bool autoplayGif() const;
  void setAutoplayGif(bool autoplay);

  bool svgBackgroundEnabled() const;
  void setSvgBackgroundEnabled(bool enabled);

  QColor svgBackground() const;
  void setSvgBackground(const QColor &color);

private:
  MediaSettings() = default;
  ~MediaSettings() = default;
  MediaSettings(const MediaSettings &) = delete;
  MediaSettings &operator=(const MediaSettings &) = delete;
};

#endif // EPISTEME_MEDIASETTINGS_H