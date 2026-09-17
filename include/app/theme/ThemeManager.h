#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>

#include "ThemeTokens.h"

class ThemeManager : public QObject {
  Q_OBJECT

public:
  explicit ThemeManager(QObject *parent = nullptr);

  bool loadTheme(const QString &name, const QString &qss);

  QString currentTheme() const;
  ThemeTokens currentTokens() const;

  signals:
    void themeChanged(const QString &name, const ThemeTokens &tokens);

private:
  QString m_currentTheme;
  ThemeTokens m_currentTokens;
};

#endif // THEMEMANAGER_H