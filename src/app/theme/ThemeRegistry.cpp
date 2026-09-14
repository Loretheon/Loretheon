#include "../../../include/app/theme/ThemeTokens.h"

#include <QRegularExpression>

namespace {

QColor parseColor(const QString &raw) {
  const QString v = raw.trimmed();
  if (v.isEmpty()) return QColor();

  QColor c(v);
  if (c.isValid()) return c;

  static const QRegularExpression rgb(
      R"(rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([\d.]+)\s*)?\))",
      QRegularExpression::CaseInsensitiveOption);
  const auto m = rgb.match(v);
  if (m.hasMatch()) {
    QColor out(m.captured(1).toInt(), m.captured(2).toInt(),
               m.captured(3).toInt());
    if (!m.captured(4).isEmpty()) {
      out.setAlphaF(m.captured(4).toDouble());
    }
    return out;
  }

  return QColor();
}

QHash<QString, QString> extractDeclarations(const QString &qss) {
  QHash<QString, QString> out;

  static const QRegularExpression block(R"(([^{}]+)\{([^{}]*)\})");
  auto it = block.globalMatch(qss);
  while (it.hasNext()) {
    const auto m = it.next();
    const QString selector = m.captured(1).trimmed();
    const QString body = m.captured(2);

    static const QRegularExpression decl(R"(([a-zA-Z-]+)\s*:\s*([^;]+);?)");
    auto dit = decl.globalMatch(body);
    while (dit.hasNext()) {
      const auto d = dit.next();
      const QString key = selector + "|" + d.captured(1).trimmed().toLower();
      out.insert(key, d.captured(2).trimmed());
    }
  }

  return out;
}

QColor lookup(const QHash<QString, QString> &decls,
              const QStringList &candidates,
              const QColor &fallback) {
  for (const QString &key : candidates) {
    auto it = decls.constFind(key);
    if (it == decls.constEnd()) continue;
    const QColor c = parseColor(it.value());
    if (c.isValid()) return c;
  }
  return fallback;
}

} // namespace

ThemeRegistry &ThemeRegistry::instance() {
  static ThemeRegistry reg;
  return reg;
}

ThemeRegistry::ThemeRegistry() {
  ThemeTokens frappe;
  frappe.base = QColor("#303446");
  frappe.mantle = QColor("#292c3c");
  frappe.crust = QColor("#232634");
  frappe.surface0 = QColor("#414559");
  frappe.surface1 = QColor("#51576d");
  frappe.surface2 = QColor("#626880");
  frappe.overlay0 = QColor("#737994");
  frappe.overlay1 = QColor("#838ba7");
  frappe.overlay2 = QColor("#949cbb");
  frappe.text = QColor("#c6d0f5");
  frappe.subtext0 = QColor("#a5adce");
  frappe.subtext1 = QColor("#b5bfe2");
  frappe.blue = QColor("#8caaee");
  frappe.lavender = QColor("#babbf1");
  frappe.sapphire = QColor("#85c1dc");
  frappe.sky = QColor("#99d1db");
  frappe.teal = QColor("#81c8be");
  frappe.green = QColor("#a6d189");
  frappe.yellow = QColor("#e5c890");
  frappe.peach = QColor("#ef9f76");
  frappe.maroon = QColor("#ea999c");
  frappe.red = QColor("#e78284");
  frappe.mauve = QColor("#ca9ee6");
  frappe.pink = QColor("#f4b8e4");
  frappe.flamingo = QColor("#eebebe");
  frappe.rosewater = QColor("#f2d5cf");

  ThemeTokens latte;
  latte.base = QColor("#eff1f5");
  latte.mantle = QColor("#e6e9ef");
  latte.crust = QColor("#dce0e8");
  latte.surface0 = QColor("#ccd0da");
  latte.surface1 = QColor("#bcc0cc");
  latte.surface2 = QColor("#acb0be");
  latte.overlay0 = QColor("#9ca0b0");
  latte.overlay1 = QColor("#8c8fa1");
  latte.overlay2 = QColor("#7c7f93");
  latte.text = QColor("#4c4f69");
  latte.subtext0 = QColor("#6c6f85");
  latte.subtext1 = QColor("#5c5f77");
  latte.blue = QColor("#1e66f5");
  latte.lavender = QColor("#7287fd");
  latte.sapphire = QColor("#209fb5");
  latte.sky = QColor("#04a5e5");
  latte.teal = QColor("#179299");
  latte.green = QColor("#40a02b");
  latte.yellow = QColor("#df8e1d");
  latte.peach = QColor("#fe640b");
  latte.maroon = QColor("#e64553");
  latte.red = QColor("#d20f39");
  latte.mauve = QColor("#8839ef");
  latte.pink = QColor("#ea76cb");
  latte.flamingo = QColor("#dd7878");
  latte.rosewater = QColor("#dc8a78");

  ThemeTokens macchiato;
  macchiato.base = QColor("#24273a");
  macchiato.mantle = QColor("#1e2030");
  macchiato.crust = QColor("#181926");
  macchiato.surface0 = QColor("#363a4f");
  macchiato.surface1 = QColor("#494d64");
  macchiato.surface2 = QColor("#5b6078");
  macchiato.overlay0 = QColor("#6e738d");
  macchiato.overlay1 = QColor("#8087a2");
  macchiato.overlay2 = QColor("#939ab7");
  macchiato.text = QColor("#cad3f5");
  macchiato.subtext0 = QColor("#a5adcb");
  macchiato.subtext1 = QColor("#b8c0e0");
  macchiato.blue = QColor("#8aadf4");
  macchiato.lavender = QColor("#b7bdf8");
  macchiato.sapphire = QColor("#7dc4e4");
  macchiato.sky = QColor("#91d7e3");
  macchiato.teal = QColor("#8bd5ca");
  macchiato.green = QColor("#a6da95");
  macchiato.yellow = QColor("#eed49f");
  macchiato.peach = QColor("#f5a97f");
  macchiato.maroon = QColor("#ee99a0");
  macchiato.red = QColor("#ed8796");
  macchiato.mauve = QColor("#c6a0f6");
  macchiato.pink = QColor("#f5bde6");
  macchiato.flamingo = QColor("#f0c6c6");
  macchiato.rosewater = QColor("#f4dbd6");

  ThemeTokens mocha;
  mocha.base = QColor("#1e1e2e");
  mocha.mantle = QColor("#181825");
  mocha.crust = QColor("#11111b");
  mocha.surface0 = QColor("#313244");
  mocha.surface1 = QColor("#45475a");
  mocha.surface2 = QColor("#585b70");
  mocha.overlay0 = QColor("#6c7086");
  mocha.overlay1 = QColor("#7f849c");
  mocha.overlay2 = QColor("#9399b2");
  mocha.text = QColor("#cdd6f4");
  mocha.subtext0 = QColor("#a6adc8");
  mocha.subtext1 = QColor("#bac2de");
  mocha.blue = QColor("#89b4fa");
  mocha.lavender = QColor("#b4befe");
  mocha.sapphire = QColor("#74c7ec");
  mocha.sky = QColor("#89dceb");
  mocha.teal = QColor("#94e2d5");
  mocha.green = QColor("#a6e3a1");
  mocha.yellow = QColor("#f9e2af");
  mocha.peach = QColor("#fab387");
  mocha.maroon = QColor("#eba0ac");
  mocha.red = QColor("#f38ba8");
  mocha.mauve = QColor("#cba6f7");
  mocha.pink = QColor("#f5c2e7");
  mocha.flamingo = QColor("#f2cdcd");
  mocha.rosewater = QColor("#f5e0dc");

  registerTokens("frappe", frappe);
  registerTokens("latte", latte);
  registerTokens("macchiato", macchiato);
  registerTokens("mocha", mocha);

  m_default = "mocha";
}

QStringList ThemeRegistry::names() const {
  return m_order;
}

bool ThemeRegistry::contains(const QString &name) const {
  return m_tokens.contains(name);
}

ThemeTokens ThemeRegistry::tokens(const QString &name) const {
  auto it = m_tokens.constFind(name);
  if (it != m_tokens.constEnd()) return it.value();
  auto def = m_tokens.constFind(m_default);
  if (def != m_tokens.constEnd()) return def.value();
  return {};
}

bool ThemeRegistry::registerTokens(const QString &name,
                                   const ThemeTokens &tokens) {
  if (name.isEmpty()) return false;
  if (!m_tokens.contains(name)) m_order.append(name);
  m_tokens.insert(name, tokens);
  return true;
}

bool ThemeRegistry::registerFromStylesheet(const QString &name,
                                           const QString &qss) {
  if (name.isEmpty() || qss.isEmpty()) return false;

  const auto decls = extractDeclarations(qss);

  ThemeTokens t = tokens(name);

  t.base = lookup(decls,
                  {"QWidget|background-color", "QMainWindow|background-color"},
                  t.base);
  t.crust = lookup(decls,
                   {"QMenuBar|background-color", "QStatusBar|background-color"},
                   t.crust);
  t.surface0 = lookup(decls,
                      {"QLineEdit|background-color",
                       "QTextEdit|background-color",
                       "QPlainTextEdit|background-color",
                       "QComboBox|background-color"},
                      t.surface0);
  t.surface1 = lookup(decls,
                      {"QMenuBar::item:selected|background-color",
                       "QMenu::item:selected|background-color",
                       "QPushButton:hover|background-color"},
                      t.surface1);
  t.surface2 = lookup(decls, {"QWidget|selection-background-color"}, t.surface2);
  t.overlay0 = lookup(decls,
                      {"QLineEdit:disabled|color",
                       "QComboBox:disabled|color"},
                      t.overlay0);
  t.overlay2 = lookup(decls,
                      {"QMenu::separator|background-color",
                       "QScrollBar::handle|background-color"},
                      t.overlay2);
  t.text = lookup(decls, {"QWidget|color", "QLabel|color"}, t.text);
  t.blue = lookup(decls,
                  {"QTabBar::tab:selected|border-bottom",
                   "QLineEdit:focus|border"},
                  t.blue);
  t.lavender = lookup(decls,
                      {"QProgressBar::chunk|background-color"},
                      t.lavender);

  m_tokens.insert(name, t);
  if (!m_order.contains(name)) m_order.append(name);
  return true;
}

QString ThemeRegistry::defaultName() const {
  return m_default;
}