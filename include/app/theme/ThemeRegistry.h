#ifndef LORE_THEMEREGISTRY_H
#define LORE_THEMEREGISTRY_H

#include "ThemeTokens.h"

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

class ThemeRegistry : public QObject {
  Q_OBJECT

public:
  static ThemeRegistry &instance();

  QStringList names() const;
  QStringList selectableNames() const;

  bool contains(const QString &name) const;

  ThemeTokens tokens(const QString &name) const;

  // Registers a theme from its stylesheet. The stylesheet begins with
  // a token block: a "@theme <name>" line followed by "@token #hex"
  // lines. All tokens are required; a theme that declares fewer than
  // the full set is rejected.
  bool registerFromStylesheet(const QString &name, const QString &qss,
                              QStringList *outMissing = nullptr);

  QString baseName() const { return QStringLiteral("lore"); }
  QString defaultNormalName() const { return QStringLiteral("normal"); }
  QString defaultOverseerName() const { return QStringLiteral("overseer"); }

  void setActiveTheme(const QString &name);
  QString activeTheme() const { return m_active; }

  QColor color(const QString &role) const;

  signals:
    void activeThemeChanged(const QString &name);

private:
  ThemeRegistry();

  static void parseTokenBlock(const QString &qss, QString *outTheme,
                              QHash<QString, QColor> *outTokens);

  QHash<QString, ThemeTokens> m_tokens;
  QStringList m_order;
  QString m_active;
};

#endif // LORE_THEMEREGISTRY_H