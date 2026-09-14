#ifndef SVGTHeMER_H
#define SVGTHeMER_H

#include <QString>

#include "ThemeTokens.h"

class SvgThemer {
public:
  static QString applyTheme(const QString &svg, const ThemeTokens &tokens);
};

#endif // SVGTHeMER_H