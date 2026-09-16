#ifndef THEMETOKENS_H
#define THEMETOKENS_H

#include <QColor>
#include <QHash>
#include <QObject>
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

class ThemeRegistry : public QObject {
  Q_OBJECT

public:
  static ThemeRegistry &instance();

  QStringList names() const;
  bool contains(const QString &name) const;
  ThemeTokens tokens(const QString &name) const;

  bool registerTokens(const QString &name, const ThemeTokens &tokens);
  bool registerFromStylesheet(const QString &name, const QString &qss);

  QString defaultName() const;

  // Active theme for semantic role lookups. Set by MainWindow when the
  // visible page's theme changes. Widgets that need a themed color
  // without receiving ThemeTokens explicitly call color(role).
  void setActiveTheme(const QString &name);
  QString activeTheme() const { return m_active; }

  // Semantic role lookup. Roles are dotted names like "event.user",
  // "badge.promoted". Unknown roles return a neutral fallback derived
  // from the active theme, never a hardcoded hex.
  QColor color(const QString &role) const;

signals:
  void activeThemeChanged(const QString &name);

private:
  ThemeRegistry();

  QHash<QString, ThemeTokens> m_tokens;
  QStringList m_order;
  QString m_default;
  QString m_active;
};

#endif // THEMETOKENS_H