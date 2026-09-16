#include "../../include/overseer/TranscriptStore.h"

#include "../../include/overseer/OverseerSession.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

namespace {

// Block separator used inside transcript.md. ASCII Record Separator is
// chosen because it cannot appear in user text, LLM output, or markdown.
// A block is:
//
//   ## <type> | <iso8601> | <json>
//   <body>
//
// followed by the separator on a line of its own.
constexpr QChar kBlockSeparator(0x1E);

QString serializeEvent(const TranscriptEvent &event) {
  QJsonObject payload;

  if (!event.toolName.isEmpty())
    payload.insert(QStringLiteral("toolName"), event.toolName);
  if (!event.toolCategory.isEmpty())
    payload.insert(QStringLiteral("toolCategory"), event.toolCategory);
  if (!event.toolArguments.isEmpty())
    payload.insert(QStringLiteral("toolArguments"), event.toolArguments);
  if (!event.toolResult.isEmpty())
    payload.insert(QStringLiteral("toolResult"), event.toolResult);
  if (!event.toolOk)
    payload.insert(QStringLiteral("toolOk"), false);
  if (event.toolDurationMs > 0)
    payload.insert(QStringLiteral("toolDurationMs"),
                   static_cast<double>(event.toolDurationMs));

  if (!event.proposalKey.isEmpty())
    payload.insert(QStringLiteral("proposalKey"), event.proposalKey);
  if (!event.proposalFact.isEmpty())
    payload.insert(QStringLiteral("proposalFact"), event.proposalFact);
  if (!event.proposalRationale.isEmpty())
    payload.insert(QStringLiteral("proposalRationale"),
                   event.proposalRationale);
  if (!event.proposalStatus.isEmpty())
    payload.insert(QStringLiteral("proposalStatus"), event.proposalStatus);

  if (!event.filePath.isEmpty())
    payload.insert(QStringLiteral("filePath"), event.filePath);

  if (event.inputTokens >= 0)
    payload.insert(QStringLiteral("inputTokens"), event.inputTokens);
  if (event.outputTokens >= 0)
    payload.insert(QStringLiteral("outputTokens"), event.outputTokens);

  const QString json = QString::fromUtf8(
      QJsonDocument(payload).toJson(QJsonDocument::Compact));

  QString block;
  block += QStringLiteral("## ");
  block += TranscriptEvent::typeToString(event.type);
  block += QStringLiteral(" | ");
  block += event.timestamp.toString(Qt::ISODateWithMs);
  block += QStringLiteral(" | ");
  block += json;
  block += QChar('\n');
  block += event.body;
  block += QChar('\n');
  block += kBlockSeparator;
  block += QChar('\n');

  return block;
}

TranscriptEvent parseHeaderless(const QString &role, const QString &body) {
  TranscriptEvent event;
  event.id = QUuid::createUuid();
  event.timestamp = QDateTime::currentDateTime();
  event.role = role;
  event.body = body;

  if (role == QStringLiteral("user"))
    event.type = TranscriptEvent::Type::UserMessage;
  else if (role == QStringLiteral("assistant"))
    event.type = TranscriptEvent::Type::AssistantMessage;
  else if (role == QStringLiteral("error"))
    event.type = TranscriptEvent::Type::Error;
  else if (role == QStringLiteral("tool"))
    event.type = TranscriptEvent::Type::ToolCall;
  else if (role == QStringLiteral("proposal"))
    event.type = TranscriptEvent::Type::MemoryProposal;
  else
    event.type = TranscriptEvent::Type::Notice;

  return event;
}

bool parseHeader(const QString &line, TranscriptEvent &event) {
  if (!line.startsWith(QStringLiteral("## ")))
    return false;

  const int firstPipe = line.indexOf(QChar('|'));

  if (firstPipe < 0)
    return false;

  const int secondPipe = line.indexOf(QChar('|'), firstPipe + 1);

  if (secondPipe < 0)
    return false;

  const QString typeString = line.mid(3, firstPipe - 3).trimmed();

  const QString timestampString =
      line.mid(firstPipe + 1, secondPipe - firstPipe - 1).trimmed();

  const QString json = line.mid(secondPipe + 1).trimmed();

  event.id = QUuid::createUuid();
  event.type = TranscriptEvent::typeFromString(typeString);
  event.timestamp =
      QDateTime::fromString(timestampString, Qt::ISODateWithMs);

  if (!event.timestamp.isValid())
    event.timestamp = QDateTime::currentDateTime();

  const QJsonObject payload =
      QJsonDocument::fromJson(json.toUtf8()).object();

  if (payload.contains(QStringLiteral("toolName")))
    event.toolName = payload.value(QStringLiteral("toolName")).toString();
  if (payload.contains(QStringLiteral("toolCategory")))
    event.toolCategory =
        payload.value(QStringLiteral("toolCategory")).toString();
  if (payload.contains(QStringLiteral("toolArguments")))
    event.toolArguments =
        payload.value(QStringLiteral("toolArguments")).toObject();
  if (payload.contains(QStringLiteral("toolResult")))
    event.toolResult =
        payload.value(QStringLiteral("toolResult")).toString();
  if (payload.contains(QStringLiteral("toolOk")))
    event.toolOk = payload.value(QStringLiteral("toolOk")).toBool();
  if (payload.contains(QStringLiteral("toolDurationMs")))
    event.toolDurationMs = static_cast<qint64>(
        payload.value(QStringLiteral("toolDurationMs")).toDouble());

  if (payload.contains(QStringLiteral("proposalKey")))
    event.proposalKey =
        payload.value(QStringLiteral("proposalKey")).toString();
  if (payload.contains(QStringLiteral("proposalFact")))
    event.proposalFact =
        payload.value(QStringLiteral("proposalFact")).toString();
  if (payload.contains(QStringLiteral("proposalRationale")))
    event.proposalRationale =
        payload.value(QStringLiteral("proposalRationale")).toString();
  if (payload.contains(QStringLiteral("proposalStatus")))
    event.proposalStatus =
        payload.value(QStringLiteral("proposalStatus")).toString();

  if (payload.contains(QStringLiteral("filePath")))
    event.filePath = payload.value(QStringLiteral("filePath")).toString();

  if (payload.contains(QStringLiteral("inputTokens")))
    event.inputTokens =
        payload.value(QStringLiteral("inputTokens")).toInt();
  if (payload.contains(QStringLiteral("outputTokens")))
    event.outputTokens =
        payload.value(QStringLiteral("outputTokens")).toInt();

  event.role = TranscriptEvent::typeToString(event.type);

  return true;
}

// Parses a legacy transcript that predates the \x1e separator. Each
// event was written as:
//
//   ## <type> | <timestamp> | <json>
//   <body possibly containing blank lines>
//   <blank line>
//
// The problem is that a body containing blank lines is ambiguous: there
// is no way to tell "blank line inside body" from "blank line between
// events" purely by looking at separators. But we do know every event
// starts with a line matching "## <type> | <timestamp> | <json>". So
// we scan for those header lines and treat everything up to the next
// header as the body of the current event. That correctly handles
// markdown bodies with arbitrary blank lines.
QList<TranscriptEvent> parseLegacyFormat(const QString &text) {
  QList<TranscriptEvent> result;

  const QStringList lines = text.split(QChar('\n'));

  static const QRegularExpression headerRe(
      QStringLiteral("^##\\s+([^|]+?)\\s*\\|\\s*([^|]+?)\\s*\\|\\s*(.*)$"));

  static const QRegularExpression legacyHeaderRe(
      QStringLiteral("^##\\s+(\\S+)\\s*(?:—|-)\\s*([^\\n]+)$"));

  int currentStart = -1;
  TranscriptEvent currentEvent;
  bool currentIsLegacy = false;

  auto flush = [&]() {
    if (currentStart < 0)
      return;

    QString body;

    for (int i = currentStart + 1; i < lines.size(); ++i) {
      // Stop at the next header. This is the body boundary.
      const QString trimmed = lines.at(i).trimmed();

      if (headerRe.match(trimmed).hasMatch() ||
          legacyHeaderRe.match(trimmed).hasMatch()) {
        break;
      }

      if (!body.isEmpty())
        body += QChar('\n');

      body += lines.at(i);
    }

    while (body.endsWith(QChar('\n')))
      body.chop(1);

    currentEvent.body = body;
    currentEvent.sequence = result.size() + 1;
    result.append(currentEvent);

    currentStart = -1;
    currentEvent = TranscriptEvent();
    currentIsLegacy = false;
  };

  for (int i = 0; i < lines.size(); ++i) {
    const QString trimmed = lines.at(i).trimmed();

    const auto modern = headerRe.match(trimmed);
    const auto legacy = legacyHeaderRe.match(trimmed);

    if (modern.hasMatch()) {
      flush();

      currentStart = i;

      if (!parseHeader(trimmed, currentEvent)) {
        currentStart = -1;
        currentEvent = TranscriptEvent();
        continue;
      }

      currentIsLegacy = false;
      continue;
    }

    if (legacy.hasMatch()) {
      flush();

      currentStart = i;

      currentEvent = parseHeaderless(legacy.captured(1).trimmed(),
                                     QString());

      const QDateTime ts =
          QDateTime::fromString(legacy.captured(2).trimmed(), Qt::ISODate);

      if (ts.isValid())
        currentEvent.timestamp = ts;

      currentIsLegacy = true;
      continue;
    }
  }

  flush();

  Q_UNUSED(currentIsLegacy);

  return result;
}

} // namespace

TranscriptStore::TranscriptStore(QObject *parent) : QObject(parent) {}

void TranscriptStore::setSession(OverseerSession *session) {
  if (m_session == session)
    return;

  m_session = session;
  loadFromDisk();
}

QString TranscriptStore::transcriptPath() const {
  if (!m_session)
    return {};

  return m_session->transcriptPath();
}

void TranscriptStore::clear() {
  m_events.clear();
  m_sequence = 0;
  emit eventsReset();
}

void TranscriptStore::loadFromDisk() {
  m_events.clear();
  m_sequence = 0;

  const QString path = transcriptPath();

  if (path.isEmpty() || !QFile::exists(path)) {
    emit eventsReset();
    return;
  }

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    emit eventsReset();
    return;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const QString text = stream.readAll();

  const bool isModern = text.contains(kBlockSeparator);

  if (isModern) {
    QStringList blocks = text.split(kBlockSeparator, Qt::SkipEmptyParts);

    for (const QString &rawBlock : blocks) {
      QString block = rawBlock;

      while (block.startsWith(QChar('\n')))
        block.remove(0, 1);

      while (block.endsWith(QChar('\n')))
        block.chop(1);

      if (block.isEmpty())
        continue;

      const int firstNewline = block.indexOf(QChar('\n'));

      const QString firstLine =
          firstNewline < 0 ? block : block.left(firstNewline);

      const QString rest =
          firstNewline < 0 ? QString() : block.mid(firstNewline + 1);

      TranscriptEvent event;

      if (parseHeader(firstLine.trimmed(), event)) {
        event.body = rest;
        event.sequence = ++m_sequence;
        m_events.append(event);
      }
    }
  } else {
    // Legacy format: no separator. Scan by header line.
    m_events = parseLegacyFormat(text);

    for (int i = 0; i < m_events.size(); ++i)
      m_events[i].sequence = static_cast<quint64>(i + 1);

    m_sequence = m_events.size();
  }

  emit eventsReset();

  // If the file was legacy, immediately rewrite it in the modern
  // format. This is a one-shot migration. Nothing about the user's
  // content is changed; only the on-disk separator and structure.
  if (!isModern && !m_events.isEmpty()) {
    rewriteDisk();
  }
}

void TranscriptStore::append(const TranscriptEvent &event) {
  TranscriptEvent copy = event;
  copy.id = copy.id.isNull() ? QUuid::createUuid() : copy.id;
  copy.sequence = ++m_sequence;

  if (!copy.timestamp.isValid())
    copy.timestamp = QDateTime::currentDateTime();

  m_events.append(copy);
  emit eventAppended(m_events.size() - 1);

  const QString path = transcriptPath();

  if (path.isEmpty())
    return;

  QFile file(path);

  if (!file.open(QIODevice::Append | QIODevice::Text))
    return;

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << serializeEvent(copy);
  stream.flush();
}

void TranscriptStore::updateProposalStatus(const QString &proposalKey,
                                           const QString &status) {
  bool changed = false;

  for (int i = 0; i < m_events.size(); ++i) {
    if (m_events[i].proposalKey != proposalKey)
      continue;

    m_events[i].proposalStatus = status;
    emit eventUpdated(i);
    changed = true;
  }

  if (changed)
    rewriteDisk();
}

void TranscriptStore::rewriteDisk() {
  const QString path = transcriptPath();

  if (path.isEmpty())
    return;

  QFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  for (const TranscriptEvent &event : std::as_const(m_events))
    stream << serializeEvent(event);

  stream.flush();
}