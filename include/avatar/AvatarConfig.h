#pragma once

#include <QSize>

// All of the avatar's on-screen sizing in one place.
//
// widgetSize is the first-run default. Once the user has resized her
// by dragging a corner grip, the chosen size is stored in QSettings
// under avatar/size and overrides this value. The framing values
// below — camera, scale, offsets — are not user-adjustable and are
// always applied from here.
struct AvatarConfig {
  // Default size on first run, before the user has resized her.
  QSize widgetSize = QSize(320, 480);

  // Resize bounds. The aspect ratio is preserved during a drag, so
  // only the tighter of the two limits is ever reached on any given
  // drag direction.
  QSize minSize = QSize(120, 180);
  QSize maxSize = QSize(640, 960);

  // Default distance from the bottom-right corner of the main window,
  // on first run. Once the user has resized her, the widget remembers
  // its own position relative to the window's bottom-right corner.
  int margin = 24;

  // Camera framing. The camera sits on the +Y/+Z side looking at the
  // origin. Larger distance or smaller field of view makes the figure
  // appear smaller inside the widget.
  float cameraDistance = 120.0f;
  float cameraHeight = 45.0f;
  float cameraPitch = -10.0f;
  float fieldOfView = 45.0f;

  // Vertical offset of the figure inside the scene, in scene units.
  // More negative pushes the figure down inside the widget.
  float figureOffsetY = -20.0f;

  // Scale applied to the loaded Vita model, in scene units.
  float modelScale = 50.0f;
};