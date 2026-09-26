#ifndef THEMETOKENS_H
#define THEMETOKENS_H

#include <QColor>
#include <QHash>
#include <QString>

struct ThemeTokens {
  // Surfaces
  QColor base;
  QColor surface0;
  QColor surface1;
  QColor surface2;
  QColor surfaceRaised;
  QColor structure;

  // Text
  QColor text;
  QColor textMuted;
  QColor textSubtle;
  QColor textDisabled;

  // Accent
  QColor accent;
  QColor accentHover;
  QColor accentPressed;
  QColor accentMuted;
  QColor accentFg;

  // Hints (pastels)
  QColor hintCool;
  QColor hintWarm;
  QColor hintNeutral;

  // Borders and dividers
  QColor border;
  QColor borderStrong;
  QColor divider;

  // States
  QColor success;
  QColor warning;
  QColor error;
  QColor info;

  // ---- Helper accessors ----
  //
  // These exist so that widget code and renderer code do not need to
  // know which semantic token backs a given visual role. They are not
  // a color vocabulary of their own; each one returns an existing
  // field. If the underlying mapping ever needs to change, change it
  // here, not at every call site.

  QColor background() const { return base; }
  QColor editorBackground() const { return surface0; }

  QColor nodeFill() const { return surfaceRaised; }
  QColor nodeStroke() const { return accentMuted; }
  QColor clusterBkg() const { return surface0; }
  QColor clusterBorder() const { return border; }
  QColor edgeStroke() const { return border; }
  QColor edgeLabelBackground() const { return base; }
  QColor markerFill() const { return border; }
  QColor textFill() const { return text; }
  QColor titleFill() const { return textMuted; }

  bool isComplete() const;
};

#endif // THEMETOKENS_H