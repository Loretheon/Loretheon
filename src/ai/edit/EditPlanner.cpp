#include "EditPlanner.h"

#include "PayloadLogger.h"
#include "edit/EditGrammar.h"
#include "inference/InferenceService.h"

#include "TextDocument.h"
#include "TextEdit.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>

#include <algorithm>

namespace {

struct SectionInfo {
  QString heading;
  QString title;
  QString scopeId;
  int start = 0;
  int end = 0;
};

QString findSectionHeading(const DocumentNode &node,
                           const QString &documentText) {
  const int start = qBound(0, node.start, documentText.size());
  const int end = qBound(start, node.end, documentText.size());

  const QStringList lines =
      documentText.mid(start, end - start).split(QChar('\n'));

  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();

    if (trimmed.startsWith(QChar('#'))) {
      return trimmed;
    }
  }

  return {};
}

QString firstNonEmptyLine(const DocumentNode &node,
                          const QString &documentText, int maxChars = 120) {
  const int start = qBound(0, node.start, documentText.size());
  const int end = qBound(start, node.end, documentText.size());

  const QStringList lines =
      documentText.mid(start, end - start).split(QChar('\n'));

  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();

    if (!trimmed.isEmpty()) {
      return trimmed.left(maxChars);
    }
  }

  return {};
}

QString headingTitle(const QString &heading) {
  const int firstNonHash =
      heading.indexOf(QRegularExpression(QStringLiteral("\\S")));

  return firstNonHash >= 0 ? heading.mid(firstNonHash).trimmed() : QString();
}

void collectSections(const DocumentNode &node, const QString &documentText,
                     QVector<SectionInfo> &sections) {
  const bool isRoot = node.id == QStringLiteral("document");

  if (!isRoot && !node.id.isEmpty()) {
    const int start = qBound(0, node.start, documentText.size());
    const int end = qBound(start, node.end, documentText.size());

    QString heading;

    if (node.type == QStringLiteral("section")) {
      heading = findSectionHeading(node, documentText);
    }

    if (heading.isEmpty()) {
      heading = firstNonEmptyLine(node, documentText);
    }

    if (!heading.isEmpty()) {
      sections.append(
          SectionInfo{heading, headingTitle(heading), node.id, start, end});
    } else {
      sections.append(
          SectionInfo{QString(), node.id, node.id, start, end});
    }
  }

  for (const DocumentNode &child : node.children) {
    collectSections(child, documentText, sections);
  }
}

SectionInfo makeDocumentRootSection(const DocumentNode &node,
                                    const QString &documentText) {
  SectionInfo section;
  section.heading = QString();
  section.title = QStringLiteral("Document");
  section.scopeId = node.id;
  section.start = 0;
  section.end = documentText.size();

  return section;
}

QVector<SectionInfo> collectSections(const DocumentStructure &structure,
                                     const QString &documentText) {
  QVector<SectionInfo> sections;

  collectSections(structure.root(), documentText, sections);

  if (sections.isEmpty() && !structure.root().id.isEmpty()) {
    sections.append(makeDocumentRootSection(structure.root(), documentText));
  }

  return sections;
}

QString hierarchyMap(const QVector<SectionInfo> &sections) {
  QStringList lines;

  for (const SectionInfo &section : sections) {
    lines.append(QStringLiteral("  %1").arg(section.scopeId));
  }

  return lines.join('\n');
}

const SectionInfo *findSection(const QVector<SectionInfo> &sections,
                               const QString &scopeId) {
  for (const SectionInfo &section : sections) {
    if (section.scopeId == scopeId) {
      return &section;
    }
  }

  return nullptr;
}

const SectionInfo *
findDocumentRootSection(const QVector<SectionInfo> &sections,
                        int documentLength) {
  for (const SectionInfo &section : sections) {
    if (section.scopeId == QStringLiteral("document") ||
        section.scopeId.endsWith(QStringLiteral(":document"))) {
      return &section;
    }
  }

  for (const SectionInfo &section : sections) {
    if (section.start == 0 && section.end == documentLength &&
        documentLength > 0) {
      return &section;
    }
  }

  return nullptr;
}

bool isDocumentRootScope(const QString &scopeId,
                         const QVector<SectionInfo> &sections,
                         int documentLength) {
  if (scopeId.isEmpty())
    return false;

  if (scopeId == QStringLiteral("document") ||
      scopeId == QStringLiteral("root") ||
      scopeId.endsWith(QStringLiteral(":document"))) {
    return true;
  }

  const SectionInfo *root = findDocumentRootSection(sections, documentLength);

  if (!root)
    return false;

  return root->scopeId == scopeId;
}

bool scopeIdsEquivalent(const QString &a, const QString &b,
                        const QVector<SectionInfo> &sections,
                        int documentLength) {
  if (a == b)
    return true;

  const bool aIsRoot = isDocumentRootScope(a, sections, documentLength);
  const bool bIsRoot = isDocumentRootScope(b, sections, documentLength);

  if (aIsRoot && bIsRoot)
    return true;

  return false;
}

QVector<QString> splitSentences(const QString &request) {
  QVector<QString> result;

  int segmentStart = 0;

  for (int i = 0; i < request.size(); ++i) {
    const QChar ch = request.at(i);

    const bool isTerminator = ch == QChar('.') || ch == QChar('!') ||
                              ch == QChar('?') || ch == QChar('\n');

    if (!isTerminator)
      continue;

    const QString segment = request.mid(segmentStart, i - segmentStart);

    if (!segment.trimmed().isEmpty())
      result.append(segment);

    segmentStart = i + 1;
  }

  if (segmentStart < request.size()) {
    const QString segment = request.mid(segmentStart);

    if (!segment.trimmed().isEmpty())
      result.append(segment);
  }

  return result;
}

bool sentenceNegatesMention(const QString &sentence, int headingOffset) {
  if (headingOffset <= 0)
    return false;

  const QString prefix = sentence.left(headingOffset).toLower();

  static const QStringList negationKeywords = {
      QStringLiteral("leave"),
      QStringLiteral("don't"),
      QStringLiteral("do not"),
      QStringLiteral("excluding"),
      QStringLiteral("except"),
      QStringLiteral("without"),
      QStringLiteral("keep"),
      QStringLiteral("untouched"),
      QStringLiteral("unchanged"),
      QStringLiteral("preserve"),
      QStringLiteral("as-is"),
      QStringLiteral("as is"),
  };

  for (const QString &keyword : negationKeywords) {
    if (prefix.contains(keyword))
      return true;
  }

  return false;
}

bool isCandidateNegated(const QString &request, const SectionInfo &section) {
  const QVector<QString> sentences = splitSentences(request);

  const QStringList mentions = {section.heading, section.title};

  bool sawMention = false;

  for (const QString &sentence : sentences) {
    for (const QString &mention : mentions) {
      if (mention.isEmpty())
        continue;

      int searchFrom = 0;

      while (true) {
        const int found =
            sentence.indexOf(mention, searchFrom, Qt::CaseInsensitive);

        if (found < 0)
          break;

        sawMention = true;

        if (!sentenceNegatesMention(sentence, found))
          return false;

        searchFrom = found + mention.size();
      }
    }
  }

  if (!sawMention)
    return false;

  return true;
}

QVector<const SectionInfo *>
resolveRequestedSections(const QString &request,
                         const QVector<SectionInfo> &sections) {
  struct Match {
    const SectionInfo *section = nullptr;
    int length = 0;
  };

  QVector<Match> matches;

  for (const SectionInfo &section : sections) {
    const QStringList candidates = {section.heading, section.title};

    int bestLength = 0;

    for (const QString &candidate : candidates) {
      if (candidate.isEmpty())
        continue;

      if (request.contains(candidate, Qt::CaseInsensitive))
        bestLength = qMax(bestLength, candidate.size());
    }

    if (bestLength > 0)
      matches.append({&section, bestLength});
  }

  std::sort(matches.begin(), matches.end(),
            [](const Match &a, const Match &b) { return a.length > b.length; });

  QVector<const SectionInfo *> result;
  QSet<QString> seen;

  for (const Match &match : matches) {
    if (seen.contains(match.section->scopeId))
      continue;

    seen.insert(match.section->scopeId);

    if (isCandidateNegated(request, *match.section))
      continue;

    result.append(match.section);
  }

  if (result.isEmpty() && sections.size() == 1)
    result.append(&sections.first());

  return result;
}

QString operationName(const EditCommand &command) {
  switch (command.operation) {
  case EditCommand::Operation::Insert:
    return QStringLiteral("insert");
  case EditCommand::Operation::Replace:
    return QStringLiteral("replace");
  case EditCommand::Operation::ReplaceScope:
    return QStringLiteral("replace_scope");
  case EditCommand::Operation::Delete:
    return QStringLiteral("delete");
  case EditCommand::Operation::Unknown:
    return QStringLiteral("unknown");
  }

  return {};
}

QString positionName(const EditCommand &command) {
  switch (command.position) {
  case EditCommand::Position::Before:
    return QStringLiteral("before");
  case EditCommand::Position::After:
    return QStringLiteral("after");
  case EditCommand::Position::Inside:
    return QStringLiteral("inside");
  }

  return {};
}

QJsonObject commandToJson(const EditCommand &command) {
  return QJsonObject{{QStringLiteral("operation"), operationName(command)},
                     {QStringLiteral("scope"), command.scopeId},
                     {QStringLiteral("position"), positionName(command)},
                     {QStringLiteral("find"), command.findString},
                     {QStringLiteral("all"), command.replaceAll},
                     {QStringLiteral("instruction"), command.instruction}};
}

QString commandsToJson(const QVector<EditCommand> &commands) {
  QJsonArray array;

  for (const EditCommand &command : commands)
    array.append(commandToJson(command));

  return QString::fromUtf8(
      QJsonDocument(array).toJson(QJsonDocument::Indented));
}

bool findExistsInScope(const EditCommand &command, const SectionInfo &section,
                       const QString &documentText) {
  if (command.findString.isEmpty())
    return false;

  const QString scopeText =
      documentText.mid(section.start, section.end - section.start);

  return scopeText.contains(command.findString);
}

bool explicitlyRequestsAll(const QString &request) {
  static const QRegularExpression pattern(
      QStringLiteral(R"(\b(all|every|each)\b)"),
      QRegularExpression::CaseInsensitiveOption);

  return pattern.match(request).hasMatch();
}

QString formatMessagesForLog(const QJsonArray &messages) {
  QString out;

  for (int i = 0; i < messages.size(); ++i) {
    const QJsonObject message = messages.at(i).toObject();
    const QString role = message.value(QStringLiteral("role")).toString();
    const QString content = message.value(QStringLiteral("content")).toString();

    out += QStringLiteral("[message %1 | role=%2]\n%3\n\n")
               .arg(i)
               .arg(role, content);
  }

  return out;
}

} // namespace

EditPlanner::EditPlanner(InferenceService *inferenceService, QObject *parent)
    : QObject(parent), m_inferenceService(inferenceService) {
  if (!m_inferenceService)
    return;

  connect(m_inferenceService, &InferenceService::llmDelta, this,
          [this](const InferenceService::RequestToken &token,
                 const QString &text) {
            if (token != m_activeToken || !m_active || text.isEmpty())
              return;

            m_streamingResponse += text;
            processStream();
          });

  connect(m_inferenceService, &InferenceService::llmFinished, this,
          [this](const InferenceService::RequestToken &token) {
            if (token != m_activeToken)
              return;

            m_activeToken = InferenceService::RequestToken();

            if (!m_active)
              return;

            m_active = false;
            disarmWatchdog();

            m_payloadLogger.log(
                QStringLiteral("EDIT_PLAN_STREAM_INCOMPLETE"),
                QStringLiteral("Received %1 bytes without a complete JSON "
                               "value.\n\nBuffer:\n%2")
                    .arg(m_streamingResponse.size())
                    .arg(m_streamingResponse));

            m_streamingResponse.clear();

            if (!m_terminalEmitted) {
              m_terminalEmitted = true;
              emit failed(
                  QStringLiteral("Edit planner did not produce a complete "
                                 "plan."));
            }
          });

  connect(m_inferenceService, &InferenceService::llmError, this,
          [this](const InferenceService::RequestToken &token,
                 const QString &error) {
            if (token != m_activeToken)
              return;

            m_activeToken = InferenceService::RequestToken();

            if (!m_active)
              return;

            m_active = false;
            disarmWatchdog();
            m_streamingResponse.clear();

            if (!m_terminalEmitted) {
              m_terminalEmitted = true;
              emit failed(QStringLiteral("LLM error: %1").arg(error));
            }
          });
}

void EditPlanner::setWatchdogMs(int ms) {
  m_watchdogMs = qBound(1000, ms, 3600000);
}

void EditPlanner::start(TextEdit *editor, const QString &userRequest) {
  start(editor, userRequest, ScopeMode::Scoped);
}

void EditPlanner::start(TextEdit *editor, const QString &userRequest,
                        ScopeMode mode) {
  if (!editor) {
    if (!m_terminalEmitted) {
      m_terminalEmitted = true;
      emit failed(QStringLiteral("No active document."));
    }
    return;
  }

  auto *document = qobject_cast<TextDocument *>(editor->document());

  if (!document) {
    if (!m_terminalEmitted) {
      m_terminalEmitted = true;
      emit failed(QStringLiteral("Active editor does not use TextDocument."));
    }
    return;
  }

  m_editor = editor;

  startOnDocument(document, userRequest, mode);
}

void EditPlanner::start(TextDocument *document, const QString &userRequest) {
  start(document, userRequest, ScopeMode::Scoped);
}

void EditPlanner::start(TextDocument *document, const QString &userRequest,
                        ScopeMode mode) {
  m_editor = nullptr;

  startOnDocument(document, userRequest, mode);
}

void EditPlanner::startOnDocument(TextDocument *document,
                                  const QString &userRequest,
                                  ScopeMode mode) {
  // Any prior terminal emission belongs to a prior start. Clear it
  // before this run can emit anything.
  m_terminalEmitted = false;

  if (!m_inferenceService) {
    m_terminalEmitted = true;
    emit failed(QStringLiteral("Inference service is unavailable."));
    return;
  }

  if (!document) {
    m_terminalEmitted = true;
    emit failed(QStringLiteral("No active document."));
    return;
  }

  if (m_active)
    abort();

  m_document = document;
  m_userRequest = userRequest.trimmed();
  m_streamingResponse.clear();
  m_scopeMode = mode;

  document->rebuildStructure();

  const QString documentText = document->toPlainText();

  const QVector<SectionInfo> sections =
      collectSections(document->structure(), documentText);

  const QVector<const SectionInfo *> targets =
      resolveRequestedSections(m_userRequest, sections);

  const QStringList scopeIds = document->structure().scopeIds();

  QStringList targetLines;

  for (const SectionInfo *section : targets) {
    targetLines.append(QStringLiteral("\"%1\" -> \"%2\"")
                           .arg(section->title, section->scopeId));
  }

  const QString selectionContext = QString();

  QString prompt;

  if (m_scopeMode == ScopeMode::WholeFile) {
    prompt =
        QStringLiteral(
            "You are planning a whole-file rewrite.\n"
            "\n"
            "Return exactly one JSON array containing exactly one edit.\n"
            "Do not return markdown fences.\n"
            "Do not return explanatory text.\n"
            "\n"
            "The single edit MUST be:\n"
            "- operation: replace_scope\n"
            "- scope: \"document\"\n"
            "- position: inside\n"
            "- find: \"\"\n"
            "- all: false\n"
            "- instruction: a description of what the new file should "
            "contain, in full detail.\n"
            "\n"
            "The application will generate the new file body from your "
            "instruction. Your instruction must therefore specify everything "
            "needed to reproduce the intended output: structure, headings, "
            "code blocks, formatting, and any content that must be preserved "
            "or changed.\n"
            "\n"
            "Do not propose any other operation. Do not propose more than "
            "one edit. Do not use replace, insert, or delete.\n"
            "\n"
            "User request:\n"
            "%1\n"
            "%2\n"
            "Current file:\n"
            "%3")
            .arg(m_userRequest, selectionContext, documentText);
  } else {
    prompt =
        QStringLiteral(
            "You are planning edits to a document.\n"
            "\n"
            "Return exactly one JSON array.\n"
            "Do not return markdown fences.\n"
            "Do not return explanatory text.\n"
            "\n"
            "The application has resolved the user's target section(s) for "
            "you. You must choose the operation for each edit yourself, "
            "based on what the user's request actually asks for.\n"
            "\n"
            "Resolved target(s):\n"
            "%1\n"
            "\n"
            "Rules:\n"
            "- One user intention produces exactly one edit.\n"
            "- Do not split one intention into multiple edits.\n"
            "- Do not use delete+insert to implement replace.\n"
            "- Do not duplicate or recreate existing subsections.\n"
            "- Preserve existing headings unless the user explicitly "
            "requests structural changes.\n"
            "- Do not target a parent or child section when the requested "
            "section itself exists.\n"
            "\n"
            "Each edit MUST contain exactly:\n"
            "operation, scope, position, find, all, instruction.\n"
            "\n"
            "Choose the operation that best matches what the user asked "
            "for:\n"
            "\n"
            "  - replace: replace a specific substring within the scope. "
            "Use this whenever the user wants to change, rewrite, update, "
            "remove-and-replace, or fix part of the scope's existing text, "
            "even if they say \"delete\" or \"remove\" while also giving "
            "replacement content. find MUST be the exact substring the "
            "user wants replaced, and it must be a verbatim substring of "
            "the scope's current text. find MUST be as short as possible "
            "while still being unique within the scope. instruction MUST "
            "describe only what the replacement of that substring should "
            "look like. position MUST be before or after.\n"
            "\n"
            "  - insert: add new content before or after a scope without "
            "removing anything. find MUST be empty. position MUST be "
            "before or after. instruction is the content to insert. Write "
            "the content the user asked for directly into the instruction "
            "field; do not describe it. Do not write a preamble such as "
            "\"Insert a new section with the following content:\". Write "
            "only the content itself.\n"
            "\n"
            "  - delete: remove a specific substring within the scope with "
            "no replacement. Use this only when the user wants content "
            "gone and nothing put in its place. find MUST be the exact "
            "substring to delete. position MUST be before or after.\n"
            "\n"
            "  - replace_scope: rewrite the entire body of a section or "
            "scope. Use this only when the user asks to rewrite or replace "
            "the ENTIRE body of a section, or when the scope has no "
            "existing body to anchor a replace against. find MUST be "
            "empty. position MUST be inside. all MUST be false.\n"
            "\n"
            "When the request names a single replacement substring and its "
            "replacement text, that is a replace, not a delete and not an "
            "insert.\n"
            "\n"
            "all MUST be false unless the user explicitly requests all, "
            "every, or each occurrence.\n"
            "\n"
            "instruction must describe exactly what this single edit "
            "should accomplish, including all requested details.\n"
            "\n"
            "Scope hierarchy:\n"
            "%2\n"
            "\n"
            "Document structure:\n"
            "%3\n"
            "\n"
            "User request:\n"
            "%4\n"
            "%5\n"
            "Document:\n"
            "%6")
            .arg(targetLines.join('\n'), hierarchyMap(sections),
                 document->structure().sectionIndexForModel(), m_userRequest,
                 selectionContext, documentText);
  }

  m_payloadLogger.log(
      QStringLiteral("EDIT_PLAN_REQUEST"),
      QStringLiteral("Mode: %1\n"
                     "User Request: \"%2\"\n"
                     "Resolved Targets (%3): %4\n"
                     "Doc Size: %5 chars | Prompt Size: %6 chars\n\n"
                     "Messages:\n%7")
          .arg(m_scopeMode == ScopeMode::WholeFile ? QStringLiteral("whole")
                                                    : QStringLiteral("scoped"),
               m_userRequest, QString::number(targets.size()),
               targetLines.isEmpty() ? QStringLiteral("None")
                                     : targetLines.join(QStringLiteral(", ")),
               QString::number(documentText.size()),
               QString::number(prompt.size()),
               formatMessagesForLog(
                   QJsonArray{QJsonObject{
                       {QStringLiteral("role"), QStringLiteral("system")},
                       {QStringLiteral("content"), prompt}}})));

  QJsonArray messages;

  messages.append(
      QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                  {QStringLiteral("content"), prompt}});

  m_active = true;

  QStringList contextScopeIds;

  if (m_scopeMode == ScopeMode::WholeFile) {
    if (!document->structure().root().id.isEmpty())
      contextScopeIds.append(document->structure().root().id);
  } else {
    for (const SectionInfo *section : targets)
      contextScopeIds.append(section->scopeId);
  }

  emit contextScopes(contextScopeIds);

  armWatchdog();

  m_activeToken = m_inferenceService->sendChatRequest(
      messages, QString(), 0.7, 120000, EditGrammar::gbnf(scopeIds));
}

void EditPlanner::abort() {
  m_active = false;
  m_streamingResponse.clear();

  disarmWatchdog();

  if (m_inferenceService && !m_activeToken.isNull())
    m_inferenceService->abortChatRequest(m_activeToken);

  m_activeToken = InferenceService::RequestToken();
}

void EditPlanner::armWatchdog() {
  if (!m_watchdog) {
    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);

    connect(m_watchdog, &QTimer::timeout, this, [this]() {
      if (!m_active)
        return;

      m_active = false;

      if (m_inferenceService && !m_activeToken.isNull())
        m_inferenceService->abortChatRequest(m_activeToken);

      m_activeToken = InferenceService::RequestToken();
      m_streamingResponse.clear();

      m_payloadLogger.log(
          QStringLiteral("EDIT_PLAN_WATCHDOG_TIMEOUT"),
          QStringLiteral("No terminal event within %1 ms. Aborting plan.")
              .arg(m_watchdogMs));

      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(QStringLiteral(
            "Edit planner timed out without producing a plan."));
      }
    });
  }

  m_watchdog->start(m_watchdogMs);
}

void EditPlanner::disarmWatchdog() {
  if (m_watchdog && m_watchdog->isActive())
    m_watchdog->stop();
}

bool EditPlanner::takeCompleteJsonValue(QString &buffer, QString &jsonText) {
  jsonText.clear();

  while (!buffer.isEmpty() && buffer.at(0).isSpace())
    buffer.remove(0, 1);

  if (buffer.isEmpty())
    return false;

  int start = 0;

  if (buffer.at(0) != QChar('{') && buffer.at(0) != QChar('[')) {
    const int objectStart = buffer.indexOf(QChar('{'));
    const int arrayStart = buffer.indexOf(QChar('['));

    if (objectStart < 0 && arrayStart < 0)
      return false;

    if (objectStart < 0) {
      start = arrayStart;
    } else if (arrayStart < 0) {
      start = objectStart;
    } else {
      start = qMin(objectStart, arrayStart);
    }

    buffer.remove(0, start);
  }

  int objectDepth = 0;
  int arrayDepth = 0;
  bool inString = false;
  bool escaped = false;

  for (int i = 0; i < buffer.size(); ++i) {
    const QChar ch = buffer.at(i);

    if (inString) {
      if (escaped) {
        escaped = false;
        continue;
      }

      if (ch == QChar('\\')) {
        escaped = true;
        continue;
      }

      if (ch == QChar('"'))
        inString = false;

      continue;
    }

    if (ch == QChar('"')) {
      inString = true;
      continue;
    }

    if (ch == QChar('{')) {
      ++objectDepth;
      continue;
    }

    if (ch == QChar('}')) {
      --objectDepth;

      if (objectDepth == 0 && arrayDepth == 0) {
        jsonText = buffer.left(i + 1);
        buffer.remove(0, i + 1);
        return true;
      }

      continue;
    }

    if (ch == QChar('[')) {
      ++arrayDepth;
      continue;
    }

    if (ch == QChar(']')) {
      --arrayDepth;

      if (objectDepth == 0 && arrayDepth == 0) {
        jsonText = buffer.left(i + 1);
        buffer.remove(0, i + 1);
        return true;
      }
    }
  }

  return false;
}

void EditPlanner::processStream() {
  if (!m_active)
    return;

  QString jsonText;

  if (!takeCompleteJsonValue(m_streamingResponse, jsonText))
    return;

  m_active = false;

  disarmWatchdog();

  if (m_inferenceService && !m_activeToken.isNull()) {
    m_inferenceService->abortChatRequest(m_activeToken);
    m_activeToken = InferenceService::RequestToken();
  }

  QJsonParseError parseError;

  const QJsonDocument json =
      QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !json.isArray()) {
    m_payloadLogger.log(QStringLiteral("EDIT_PLAN_PARSE_ERROR"),
                        QStringLiteral("Error: %1 | Received JSON: %2")
                            .arg(parseError.errorString(), jsonText));

    if (!m_terminalEmitted) {
      m_terminalEmitted = true;
      emit failed(
          QStringLiteral("Invalid edit plan: %1").arg(parseError.errorString()));
    }

    m_streamingResponse.clear();
    return;
  }

  if (!m_document) {
    if (!m_terminalEmitted) {
      m_terminalEmitted = true;
      emit failed(QStringLiteral("No active document."));
    }
    return;
  }

  m_document->rebuildStructure();

  const DocumentStructure &structure = m_document->structure();

  const QString documentText = m_document->toPlainText();
  const int documentLength = documentText.size();

  const QVector<SectionInfo> sections =
      collectSections(structure, documentText);

  const QVector<const SectionInfo *> targets =
      resolveRequestedSections(m_userRequest, sections);

  const QJsonArray items = json.array();

  QVector<EditCommand> edits;
  edits.reserve(items.size());

  for (int i = 0; i < items.size(); ++i) {
    const QJsonObject object = items.at(i).toObject();

    if (object.isEmpty()) {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(
            QStringLiteral("Edit plan item %1 is not an object.").arg(i + 1));
      }
      return;
    }

    const QJsonValue operationValue = object.value(QStringLiteral("operation"));
    const QJsonValue scopeValue = object.value(QStringLiteral("scope"));
    const QJsonValue positionValue = object.value(QStringLiteral("position"));
    const QJsonValue findValue = object.value(QStringLiteral("find"));
    const QJsonValue allValue = object.value(QStringLiteral("all"));
    const QJsonValue instructionValue =
        object.value(QStringLiteral("instruction"));

    if (!operationValue.isString() || !scopeValue.isString() ||
        !positionValue.isString() || !findValue.isString() ||
        !allValue.isBool() || !instructionValue.isString()) {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(
            QStringLiteral("Edit plan item %1 is missing a required field.")
                .arg(i + 1));
      }
      return;
    }

    const QString operation = operationValue.toString().trimmed().toLower();
    const QString position = positionValue.toString().trimmed().toLower();
    const QString scopeId = scopeValue.toString().trimmed();

    if (operation.isEmpty()) {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(QStringLiteral(
            "Edit plan item %1 has an empty operation.").arg(i + 1));
      }
      return;
    }

    if (position.isEmpty()) {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(QStringLiteral(
            "Edit plan item %1 has an empty position.").arg(i + 1));
      }
      return;
    }

    EditCommand command;

    if (operation == QStringLiteral("insert")) {
      command.operation = EditCommand::Operation::Insert;
    } else if (operation == QStringLiteral("replace")) {
      command.operation = EditCommand::Operation::Replace;
    } else if (operation == QStringLiteral("delete")) {
      command.operation = EditCommand::Operation::Delete;
    } else if (operation == QStringLiteral("replace_scope")) {
      command.operation = EditCommand::Operation::ReplaceScope;
    } else {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(QStringLiteral("Edit plan item %1 has an invalid operation.")
                        .arg(i + 1));
      }
      return;
    }

    if (position == QStringLiteral("before")) {
      command.position = EditCommand::Position::Before;
    } else if (position == QStringLiteral("after")) {
      command.position = EditCommand::Position::After;
    } else if (position == QStringLiteral("inside")) {
      command.position = EditCommand::Position::Inside;
    } else {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(QStringLiteral("Edit plan item %1 has an invalid position.")
                        .arg(i + 1));
      }
      return;
    }

    command.scopeId = scopeId;
    command.findString = findValue.toString();
    command.replaceAll = allValue.toBool();
    command.instruction = instructionValue.toString().trimmed();

    if (m_scopeMode == ScopeMode::WholeFile) {
      if (command.operation != EditCommand::Operation::ReplaceScope) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral(
              "Whole-file mode requires operation 'replace_scope'."));
        }
        return;
      }
      if (command.scopeId != QStringLiteral("document")) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral(
              "Whole-file mode requires scope 'document'."));
        }
        return;
      }
      if (command.position != EditCommand::Position::Inside) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral(
              "Whole-file mode requires position 'inside'."));
        }
        return;
      }
      if (!command.findString.isEmpty()) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral(
              "Whole-file mode requires an empty find string."));
        }
        return;
      }
      if (command.replaceAll) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral(
              "Whole-file mode requires all=false."));
        }
        return;
      }
    }

    const bool isDocumentRoot =
        isDocumentRootScope(command.scopeId, sections, documentLength);

    const SectionInfo *section = findSection(sections, command.scopeId);

    if (!isDocumentRoot && !section && command.scopeId.isEmpty()) {
      if (command.operation != EditCommand::Operation::Insert) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral("Edit plan item %1 uses an empty scope but "
                                     "is not an insertion.")
                          .arg(i + 1));
        }
        return;
      }

      if (!documentText.isEmpty()) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral("Edit plan item %1 uses an empty scope but "
                                     "the document is not empty.")
                          .arg(i + 1));
        }
        return;
      }
    } else if (!isDocumentRoot && !section) {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(
            QStringLiteral("Edit plan item %1 contains an invalid scope ID: %2")
                .arg(i + 1)
                .arg(command.scopeId));
      }
      return;
    }

    if (command.replaceAll && !explicitlyRequestsAll(m_userRequest)) {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(
            QStringLiteral("Edit plan item %1 uses all=true without an explicit "
                           "request to affect all occurrences.")
                .arg(i + 1));
      }
      return;
    }

    if (m_scopeMode == ScopeMode::Scoped && !targets.isEmpty()) {
      bool validTarget = false;

      for (const SectionInfo *target : targets) {
        if (scopeIdsEquivalent(target->scopeId, command.scopeId, sections,
                               documentLength)) {
          validTarget = true;
          break;
        }
      }

      if (!validTarget) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(
              QStringLiteral("Edit plan item %1 targets a section that was "
                             "not requested by the user.")
                  .arg(i + 1));
        }
        return;
      }
    }

    if (command.operation == EditCommand::Operation::Insert) {
      if (!command.findString.isEmpty()) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral("Edit plan item %1 is an insert but has a "
                                     "non-empty find string.")
                          .arg(i + 1));
        }
        return;
      }

      if (command.replaceAll) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(
              QStringLiteral("Edit plan item %1 is an insert with all=true.")
                  .arg(i + 1));
        }
        return;
      }
    } else if (command.operation == EditCommand::Operation::ReplaceScope) {
      if (!command.findString.isEmpty()) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(
              QStringLiteral("Edit plan item %1 is a replace_scope but has a "
                             "non-empty find string.")
                  .arg(i + 1));
        }
        return;
      }

      if (command.position != EditCommand::Position::Inside) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(
              QStringLiteral("Edit plan item %1 is a replace_scope but its "
                             "position is not inside.")
                  .arg(i + 1));
        }
        return;
      }

      if (command.replaceAll) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(
              QStringLiteral("Edit plan item %1 is a replace_scope with "
                             "all=true.")
                  .arg(i + 1));
        }
        return;
      }
    } else {
      if (command.findString.trimmed().isEmpty()) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(
              QStringLiteral("Edit plan item %1 requires find text.").arg(i + 1));
        }
        return;
      }

      if (!isDocumentRoot &&
          (!section || !findExistsInScope(command, *section, documentText))) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral("Edit plan item %1 uses find text that does "
                                     "not exist in the selected scope.")
                          .arg(i + 1));
        }
        return;
      }
    }

    if (command.instruction.isEmpty()) {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(QStringLiteral("Edit plan item %1 has an empty instruction.")
                        .arg(i + 1));
      }
      return;
    }

    if (!command.isCommandValid()) {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(
            QStringLiteral("Edit plan item %1 violates the edit protocol.")
                .arg(i + 1));
      }
      return;
    }

    edits.append(command);
  }

  if (m_scopeMode == ScopeMode::WholeFile) {
    if (edits.size() != 1) {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(QStringLiteral(
            "Whole-file mode requires exactly one edit."));
      }
      return;
    }
  } else if (!targets.isEmpty()) {
    if (edits.isEmpty()) {
      if (!m_terminalEmitted) {
        m_terminalEmitted = true;
        emit failed(QStringLiteral("No edits required."));
      }
      return;
    }

    QSet<QString> seenScopes;

    for (const EditCommand &edit : edits) {
      QString key = edit.scopeId;

      if (key.isEmpty()) {
        for (const SectionInfo &s : sections) {
          if (s.start == 0) {
            key = s.scopeId;
            break;
          }
        }
      }

      if (seenScopes.contains(key)) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral(
              "The planner produced multiple edits for the same section."));
        }
        return;
      }

      seenScopes.insert(key);
    }

    for (const QString &seen : seenScopes) {
      bool coversTarget = false;

      for (const SectionInfo *target : targets) {
        if (scopeIdsEquivalent(target->scopeId, seen, sections,
                               documentLength)) {
          coversTarget = true;
          break;
        }
      }

      if (!coversTarget) {
        if (!m_terminalEmitted) {
          m_terminalEmitted = true;
          emit failed(QStringLiteral(
              "The planner targeted a section the user did not name."));
        }
        return;
      }
    }
  }

  m_payloadLogger.log(QStringLiteral("EDIT_PLAN_VALIDATED"),
                      commandsToJson(edits));

  m_streamingResponse.clear();

  if (edits.isEmpty()) {
    if (!m_terminalEmitted) {
      m_terminalEmitted = true;
      emit failed(QStringLiteral("No edits required."));
    }
    return;
  }

  QStringList referencedScopes;
  QSet<QString> seenReferenced;

  for (const EditCommand &edit : edits) {
    if (edit.scopeId.isEmpty())
      continue;

    if (seenReferenced.contains(edit.scopeId))
      continue;

    seenReferenced.insert(edit.scopeId);
    referencedScopes.append(edit.scopeId);
  }

  emit contextScopes(referencedScopes);

  if (m_terminalEmitted)
    return;

  m_terminalEmitted = true;

  QMetaObject::invokeMethod(
      this, [this, edits]() { emit planValidated(edits); },
      Qt::QueuedConnection);
}