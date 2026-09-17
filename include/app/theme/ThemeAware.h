#ifndef THEMEAWARE_H
#define THEMEAWARE_H

#include "ThemeTokens.h"

class ThemeAware {
public:
  virtual ~ThemeAware() = default;
  virtual void setThemeTokens(const ThemeTokens &tokens) = 0;
};

#endif // THEMEAWARE_H