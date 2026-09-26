#include "ThemeRegistry.h"

#include <QRegularExpression>

namespace {

const QStringList &allTokens() {
  static const QStringList tokens = {
      QStringLiteral("base"),
      QStringLiteral("surface0"),
      QStringLiteral("surface1"),
      QStringLiteral("surface2"),
      QStringLiteral("surface-raised"),
      QStringLiteral("structure"),

      QStringLiteral("text"),
      QStringLiteral("text-muted"),
      QStringLiteral("text-subtle"),
      QStringLiteral("text-disabled"),

      QStringLiteral("accent"),
      QStringLiteral("accent-hover"),
      QStringLiteral("accent-pressed"),
      QStringLiteral("accent-muted"),
      QStringLiteral("accent-fg"),

      QStringLiteral("hint-cool"),
      QStringLiteral("hint-warm"),
      QStringLiteral("hint-neutral"),

      QStringLiteral("border"),
      QStringLiteral("border-strong"),
      QStringLiteral("divider"),

      QStringLiteral("success"),
      QStringLiteral("warning"),
      QStringLiteral("error"),
      QStringLiteral("info"),
  };
  return tokens;
}

void setField(ThemeTokens &t, const QString &field, const QColor &color) {
  if (!color.isValid())
    return;

  if (field == QStringLiteral("base")) { t.base = color; return; }
  if (field == QStringLiteral("surface0")) { t.surface0 = color; return; }
  if (field == QStringLiteral("surface1")) { t.surface1 = color; return; }
  if (field == QStringLiteral("surface2")) { t.surface2 = color; return; }
  if (field == QStringLiteral("surface-raised")) { t.surfaceRaised = color; return; }
  if (field == QStringLiteral("structure")) { t.structure = color; return; }

  if (field == QStringLiteral("text")) { t.text = color; return; }
  if (field == QStringLiteral("text-muted")) { t.textMuted = color; return; }
  if (field == QStringLiteral("text-subtle")) { t.textSubtle = color; return; }
  if (field == QStringLiteral("text-disabled")) { t.textDisabled = color; return; }

  if (field == QStringLiteral("accent")) { t.accent = color; return; }
  if (field == QStringLiteral("accent-hover")) { t.accentHover = color; return; }
  if (field == QStringLiteral("accent-pressed")) { t.accentPressed = color; return; }
  if (field == QStringLiteral("accent-muted")) { t.accentMuted = color; return; }
  if (field == QStringLiteral("accent-fg")) { t.accentFg = color; return; }

  if (field == QStringLiteral("hint-cool")) { t.hintCool = color; return; }
  if (field == QStringLiteral("hint-warm")) { t.hintWarm = color; return; }
  if (field == QStringLiteral("hint-neutral")) { t.hintNeutral = color; return; }

  if (field == QStringLiteral("border")) { t.border = color; return; }
  if (field == QStringLiteral("border-strong")) { t.borderStrong = color; return; }
  if (field == QStringLiteral("divider")) { t.divider = color; return; }

  if (field == QStringLiteral("success")) { t.success = color; return; }
  if (field == QStringLiteral("warning")) { t.warning = color; return; }
  if (field == QStringLiteral("error")) { t.error = color; return; }
  if (field == QStringLiteral("info")) { t.info = color; return; }
}

QColor fieldFor(const ThemeTokens &t, const QString &field) {
  if (field == QStringLiteral("base")) return t.base;
  if (field == QStringLiteral("surface0")) return t.surface0;
  if (field == QStringLiteral("surface1")) return t.surface1;
  if (field == QStringLiteral("surface2")) return t.surface2;
  if (field == QStringLiteral("surface-raised")) return t.surfaceRaised;
  if (field == QStringLiteral("structure")) return t.structure;

  if (field == QStringLiteral("text")) return t.text;
  if (field == QStringLiteral("text-muted")) return t.textMuted;
  if (field == QStringLiteral("text-subtle")) return t.textSubtle;
  if (field == QStringLiteral("text-disabled")) return t.textDisabled;

  if (field == QStringLiteral("accent")) return t.accent;
  if (field == QStringLiteral("accent-hover")) return t.accentHover;
  if (field == QStringLiteral("accent-pressed")) return t.accentPressed;
  if (field == QStringLiteral("accent-muted")) return t.accentMuted;
  if (field == QStringLiteral("accent-fg")) return t.accentFg;

  if (field == QStringLiteral("hint-cool")) return t.hintCool;
  if (field == QStringLiteral("hint-warm")) return t.hintWarm;
  if (field == QStringLiteral("hint-neutral")) return t.hintNeutral;

  if (field == QStringLiteral("border")) return t.border;
  if (field == QStringLiteral("border-strong")) return t.borderStrong;
  if (field == QStringLiteral("divider")) return t.divider;

  if (field == QStringLiteral("success")) return t.success;
  if (field == QStringLiteral("warning")) return t.warning;
  if (field == QStringLiteral("error")) return t.error;
  if (field == QStringLiteral("info")) return t.info;

  return QColor();
}

} // namespace

bool ThemeTokens::isComplete() const {
  return base.isValid() && surface0.isValid() && surface1.isValid() &&
         surface2.isValid() && surfaceRaised.isValid() && structure.isValid() &&
         text.isValid() && textMuted.isValid() && textSubtle.isValid() &&
         textDisabled.isValid() && accent.isValid() && accentHover.isValid() &&
         accentPressed.isValid() && accentMuted.isValid() &&
         accentFg.isValid() && hintCool.isValid() && hintWarm.isValid() &&
         hintNeutral.isValid() && border.isValid() && borderStrong.isValid() &&
         divider.isValid() && success.isValid() && warning.isValid() &&
         error.isValid() && info.isValid();
}

ThemeRegistry &ThemeRegistry::instance() {
  static ThemeRegistry reg;
  return reg;
}

ThemeRegistry::ThemeRegistry() = default;

QStringList ThemeRegistry::names() const { return m_order; }

QStringList ThemeRegistry::selectableNames() const {
  QStringList result;

  for (const QString &name : m_order) {
    if (name == baseName())
      continue;
    result.append(name);
  }

  return result;
}

bool ThemeRegistry::contains(const QString &name) const {
  return m_tokens.contains(name);
}

ThemeTokens ThemeRegistry::tokens(const QString &name) const {
  auto it = m_tokens.constFind(name);
  if (it != m_tokens.constEnd())
    return it.value();

  return {};
}

void ThemeRegistry::parseTokenBlock(const QString &qss, QString *outTheme,
                                    QHash<QString, QColor> *outTokens) {
  if (outTheme)
    outTheme->clear();

  if (outTokens)
    outTokens->clear();

  static const QRegularExpression tokenLine(
      QStringLiteral(
          R"(^\s*@([A-Za-z][A-Za-z0-9-]*)\s+(#[0-9A-Fa-f]{3,8})\s*$)"),
      QRegularExpression::NoPatternOption);

  static const QRegularExpression themeLine(
      QStringLiteral(R"(^\s*@theme\s+(\S+)\s*$)"),
      QRegularExpression::NoPatternOption);

  const QStringList lines = qss.split(QChar('\n'));

  bool inBlockComment = false;

  for (const QString &rawLine : lines) {
    const QString line = rawLine.trimmed();

    if (inBlockComment) {
      if (line.contains(QStringLiteral("*/")))
        inBlockComment = false;
      continue;
    }

    if (line.isEmpty())
      continue;

    if (line.startsWith(QStringLiteral("/*"))) {
      if (!line.contains(QStringLiteral("*/")))
        inBlockComment = true;
      continue;
    }

    if (line.startsWith(QStringLiteral("//")))
      continue;

    const auto themeMatch = themeLine.match(line);

    if (themeMatch.hasMatch()) {
      if (outTheme)
        *outTheme = themeMatch.captured(1).trimmed();
      continue;
    }

    const auto tokenMatch = tokenLine.match(line);

    if (tokenMatch.hasMatch()) {
      const QString name = tokenMatch.captured(1).trimmed().toLower();
      const QColor color(tokenMatch.captured(2));

      if (color.isValid() && outTokens)
        outTokens->insert(name, color);

      continue;
    }

    // First non-token, non-comment, non-blank line ends the preamble.
    break;
  }
}

bool ThemeRegistry::registerFromStylesheet(const QString &name,
                                           const QString &qss,
                                           QStringList *outMissing) {
  if (outMissing)
    outMissing->clear();

  if (qss.isEmpty())
    return false;

  QString themeName;
  QHash<QString, QColor> parsed;

  parseTokenBlock(qss, &themeName, &parsed);

  if (themeName.isEmpty()) {
    qWarning() << "[ThemeRegistry] No @theme line in stylesheet" << name;
    return false;
  }

  ThemeTokens tokens;
  QStringList missing;

  for (const QString &required : allTokens()) {
    const auto it = parsed.constFind(required);

    if (it == parsed.constEnd() || !it.value().isValid()) {
      missing.append(required);
      continue;
    }

    setField(tokens, required, it.value());
  }

  if (!missing.isEmpty()) {
    if (outMissing)
      *outMissing = missing;

    qWarning() << "[ThemeRegistry] Theme" << themeName
               << "is missing tokens:" << missing;

    return false;
  }

  if (!tokens.isComplete()) {
    qWarning() << "[ThemeRegistry] Theme" << themeName
               << "produced an incomplete token set.";
    return false;
  }

  if (!m_tokens.contains(themeName))
    m_order.append(themeName);

  m_tokens.insert(themeName, tokens);

  return true;
}

void ThemeRegistry::setActiveTheme(const QString &name) {
  if (name.isEmpty() || name == m_active)
    return;

  m_active = name;
  emit activeThemeChanged(m_active);
}

QColor ThemeRegistry::color(const QString &role) const {
  static const QHash<QString, QString> roleToField = {
      {"event.user", "accent"},
      {"event.assistant", "hint-neutral"},
      {"event.tool.ok", "success"},
      {"event.tool.error", "error"},
      {"event.proposal", "hint-cool"},
      {"event.stage", "warning"},
      {"event.promotion", "success"},
      {"event.error", "error"},
      {"event.notice", "text-muted"},

      {"reference.present", "success"},
      {"reference.missing", "error"},

      {"badge.ai", "hint-neutral"},
      {"badge.staged", "warning"},
      {"badge.promoted", "success"},
      {"badge.missing", "error"},
      {"badge.editor", "accent"},

      {"accent", "accent"},
      {"accent.hover", "accent-hover"},
      {"accent.pressed", "accent-pressed"},
      {"muted", "text-muted"},
      {"muted.strong", "text-subtle"},
      {"surface", "surface0"},
      {"surface.alt", "surface-raised"},
      {"border", "border"},
      {"divider", "divider"},

      {"notification.info", "info"},
      {"notification.warning", "warning"},
      {"notification.error", "error"},
      {"notification.critical", "error"},
  };

  const ThemeTokens t = tokens(m_active);

  const auto it = roleToField.constFind(role);

  if (it == roleToField.constEnd())
    return t.textMuted;

  return fieldFor(t, it.value());
}