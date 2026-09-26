#pragma once

#include <QSize>

struct AvatarConfig {
  QSize widgetSize = QSize(480, 720);
  QSize minSize = QSize(120, 180);
  QSize maxSize = QSize(960, 1440);
  int margin = 24;
};