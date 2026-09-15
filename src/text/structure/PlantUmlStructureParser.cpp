#include "../../../include/text/structure/PlantUmlStructureParser.h"

#include <QCryptographicHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace {

constexpr int StableHashLength = 6;

struct Match {
  QString kind;
  QString name;
  int start;
  int end;
};

QVector<Match> scanEntities(const QString &text) {
  QVector<Match> matches;

  static const QRegularExpression actorRe(
      QStringLiteral(R"(^\s*(actor|participant|boundary|control|entity|database|collections|queue)\s+(?:\"([^\"]+)\"|([A-Za-z_][A-Za-z0-9_]*)))"),
      QRegularExpression::MultilineOption);

  static const QRegularExpression messageRe(
      QStringLiteral(R"(^\s*([A-Za-z_][A-Za-z0-9_]*|\"[^\"]+\")\s*(-+|->|-->|\.\.>|<-|<-+|\)?\s*)\s*([A-Za-z_][A-Za-z0-9_]*|\"[^\"]+\")\s*:\s*(.+)$)"),
      QRegularExpression::MultilineOption);

  auto collect = [&](const QRegularExpression &re, const QString &kind) {
    auto it = re.globalMatch(text);

    while (it.hasNext()) {
      const QRegularExpressionMatch m = it.next();

      Match match;
      match.kind = kind;

      if (kind == QStringLiteral("actor")) {
        match.name = m.captured(2).isEmpty() ? m.captured(3) : m.captured(2);
      } else {
        match.name = m.captured(0).trimmed();
      }

      match.start = m.capturedStart(0);
      match.end = m.capturedEnd(0);

      if (!match.name.isEmpty()) {
        matches.append(match);
      }
    }
  };

  collect(actorRe, QStringLiteral("actor"));
  collect(messageRe, QStringLiteral("message"));

  return matches;
}

} // namespace

QString PlantUmlStructureParser::slugify(const QString &text) {
  QString result = text.trimmed();

  result = result.toLower();

  static const QRegularExpression nonAlnum(QStringLiteral("[^a-z0-9]+"));

  result.replace(nonAlnum, QStringLiteral("-"));

  while (result.startsWith(QChar('-'))) {
    result.remove(0, 1);
  }

  while (result.endsWith(QChar('-'))) {
    result.chop(1);
  }

  if (result.isEmpty()) {
    result = QStringLiteral("entity");
  }

  return result;
}

QString PlantUmlStructureParser::makeStableId(const QString &kind,
                                              const QString &slug) {
  const QByteArray hashInput =
      (kind + QStringLiteral("|") + slug).toUtf8();

  const QByteArray digest =
      QCryptographicHash::hash(hashInput, QCryptographicHash::Sha1);

  const QString hashHex =
      QString::fromLatin1(digest.toHex()).left(StableHashLength);

  return QStringLiteral("puml:%1:%2#%3").arg(kind, slug, hashHex);
}

DocumentStructure PlantUmlStructureParser::parse(const QString &text) const {
  DocumentStructure structure;
  structure.setText(text);

  DocumentNode root;

  root.id = QStringLiteral("document");
  root.type = QStringLiteral("document");
  root.start = 0;
  root.end = text.size();

  const QByteArray rootBytes = text.toUtf8();

  const QByteArray rootDigest =
      QCryptographicHash::hash(rootBytes, QCryptographicHash::Sha1);

  root.contentHash = QString::fromLatin1(rootDigest.toHex()).left(12);

  const QVector<Match> entities = scanEntities(text);

  for (const Match &match : entities) {
    DocumentNode node;

    node.type = match.kind;
    node.start = match.start;
    node.end = match.end;
    node.id = makeStableId(match.kind, slugify(match.name));

    const QByteArray bytes = text.mid(match.start, match.end - match.start).toUtf8();

    const QByteArray digest =
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha1);

    node.contentHash = QString::fromLatin1(digest.toHex()).left(12);

    root.children.append(std::move(node));
  }

  structure.setRoot(std::move(root));
  return structure;
}