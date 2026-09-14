#ifndef THEMETOKENS_H
#define THEMETOKENS_H

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>

struct ThemeTokens {
  QColor base;
  QColor mantle;
  QColor crust;
  QColor surface0;
  QColor surface1;
  QColor surface2;
  QColor overlay0;
  QColor overlay1;
  QColor overlay2;
  QColor text;
  QColor subtext0;
  QColor subtext1;
  QColor blue;
  QColor lavender;
  QColor sapphire;
  QColor sky;
  QColor teal;
  QColor green;
  QColor yellow;
  QColor peach;
  QColor maroon;
  QColor red;
  QColor mauve;
  QColor pink;
  QColor flamingo;
  QColor rosewater;

  QColor background() const { return base; }
  QColor editorBackground() const { return surface0; }
  QColor nodeFill() const { return surface0; }
  QColor nodeStroke() const { return blue; }
  QColor clusterBkg() const { return mantle; }
  QColor clusterBorder() const { return overlay0; }
  QColor edgeStroke() const { return overlay2; }
  QColor edgeLabelBackground() const { return base; }
  QColor markerFill() const { return overlay2; }
  QColor textFill() const { return text; }
  QColor titleFill() const { return subtext1; }
};

class ThemeRegistry {
public:
  static ThemeRegistry &instance();

  QStringList names() const;
  bool contains(const QString &name) const;
  ThemeTokens tokens(const QString &name) const;

  bool registerTokens(const QString &name, const ThemeTokens &tokens);
  bool registerFromStylesheet(const QString &name, const QString &qss);

  QString defaultName() const;

private:
  ThemeRegistry();

  QHash<QString, ThemeTokens> m_tokens;
  QStringList m_order;
  QString m_default;
};

#endif // THEMETOKENS_H