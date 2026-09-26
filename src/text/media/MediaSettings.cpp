#include "../../../include/text/media/MediaSettings.h"

#include <QSettings>

namespace {

constexpr auto kFitModeKey = "media/image/fitMode";
constexpr auto kZoomKey = "media/image/zoom";
constexpr auto kLockAspectKey = "media/image/lockAspect";
constexpr auto kAutoplayGifKey = "media/gif/autoplay";
constexpr auto kSvgBackgroundKey = "media/svg/background";
constexpr auto kSvgBackgroundEnabledKey = "media/svg/backgroundEnabled";
} // namespace

MediaSettings &MediaSettings::instance() {
  static MediaSettings settings;
  return settings;
}

MediaSettings::FitMode MediaSettings::imageFitMode() const {
  QSettings settings;
  const int raw = settings.value(kFitModeKey,
                                 static_cast<int>(FitMode::FitToWindow))
                      .toInt();
  if (raw < static_cast<int>(FitMode::FitToWindow) ||
      raw > static_cast<int>(FitMode::CustomZoom)) {
    return FitMode::FitToWindow;
  }
  return static_cast<FitMode>(raw);
}

void MediaSettings::setImageFitMode(FitMode mode) {
  QSettings settings;
  settings.setValue(kFitModeKey, static_cast<int>(mode));
}

double MediaSettings::imageZoom() const {
  QSettings settings;
  return settings.value(kZoomKey, 1.0).toDouble();
}

void MediaSettings::setImageZoom(double zoom) {
  QSettings settings;
  settings.setValue(kZoomKey, zoom);
}

bool MediaSettings::lockAspectRatio() const {
  QSettings settings;
  return settings.value(kLockAspectKey, true).toBool();
}

void MediaSettings::setLockAspectRatio(bool locked) {
  QSettings settings;
  settings.setValue(kLockAspectKey, locked);
}

bool MediaSettings::autoplayGif() const {
  QSettings settings;
  return settings.value(kAutoplayGifKey, true).toBool();
}

void MediaSettings::setAutoplayGif(bool autoplay) {
  QSettings settings;
  settings.setValue(kAutoplayGifKey, autoplay);
}

QColor MediaSettings::svgBackground() const {
  QSettings settings;
  const QString stored = settings.value(kSvgBackgroundKey).toString();
  if (stored.isEmpty()) {
    return QColor(Qt::white);
  }
  const QColor color(stored);
  return color.isValid() ? color : QColor(Qt::white);
}

void MediaSettings::setSvgBackground(const QColor &color) {
  QSettings settings;
  settings.setValue(kSvgBackgroundKey, color.name(QColor::HexArgb));
}
bool MediaSettings::svgBackgroundEnabled() const {
  QSettings settings;
  return settings.value(kSvgBackgroundEnabledKey, true).toBool();
}

void MediaSettings::setSvgBackgroundEnabled(bool enabled) {
  QSettings settings;
  settings.setValue(kSvgBackgroundEnabledKey, enabled);
}
