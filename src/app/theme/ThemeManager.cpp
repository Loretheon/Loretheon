#include "../../../include/app/theme/ThemeManager.h"

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {}

bool ThemeManager::loadTheme(const QString &name, const QString &qss) {
  if (name.isEmpty() || qss.isEmpty()) return false;

  ThemeRegistry::instance().registerFromStylesheet(name, qss);

  m_currentTheme = name;
  m_currentTokens = ThemeRegistry::instance().tokens(name);

  emit themeChanged(m_currentTheme, m_currentTokens);
  return true;
}

QString ThemeManager::currentTheme() const { return m_currentTheme; }

ThemeTokens ThemeManager::currentTokens() const { return m_currentTokens; }