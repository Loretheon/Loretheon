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

#include <algorithm>

namespace {

struct SectionInfo {
  QString heading;
  QString title;
  QString scopeId;
  int start = 0;
  int end = 0;
};

enum class RequestedOperation { Insert, Replace, ReplaceScope, Delete, Unknown };

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

  if (sections.isEmpty()) {
    const DocumentNode &root = structure.root();

    if (!root.id.isEmpty()) {
      sections.append(makeDocumentRootSection(root, documentText));
    }
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

bool scopeIdsEquivalent(const QString &a, const QString &b,
                        const QVector<SectionInfo> &sections) {
  if (a == b) {
    return true;
  }

  const auto isRootSection = [](const SectionInfo &s) {
    return s.start == 0;
  };

  if (a.isEmpty()) {
    for (const SectionInfo &s : sections) {
      if (s.scopeId == b) {
        return isRootSection(s);
      }
    }
    return false;
  }

  if (b.isEmpty()) {
    for (const SectionInfo &s : sections) {
      if (s.scopeId == a) {
        return isRootSection(s);
      }
    }
    return false;
  }

  return false;
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
      if (candidate.isEmpty()) {
        continue;
      }

      if (request.contains(candidate, Qt::CaseInsensitive)) {
        bestLength = qMax(bestLength, candidate.size());
      }
    }

    if (bestLength > 0) {
      matches.append({&section, bestLength});
    }
  }

  std::sort(matches.begin(), matches.end(),
            [](const Match &a, const Match &b) { return a.length > b.length; });

  QVector<const SectionInfo *> result;
  QSet<QString> seen;

  for (const Match &match : matches) {
    if (seen.contains(match.section->scopeId)) {
      continue;
    }

    seen.insert(match.section->scopeId);
    result.append(match.section);
  }

  return result;
}

RequestedOperation requestedOperation(const QString &request) {
  static const QRegularExpression deletePattern(
      QStringLiteral(R"(\b(delete|remove|erase|drop)\b)"),
      QRegularExpression::CaseInsensitiveOption);

  static const QRegularExpression replaceScopePattern(
      QStringLiteral(
          R"(\b(replace|rewrite|regenerate)\s+(the\s+)?(entire|whole|complete|full|section|body|content)\b)"),
      QRegularExpression::CaseInsensitiveOption);

  static const QRegularExpression insertPattern(
      QStringLiteral(R"(\b(insert|add|append|create)\b)"),
      QRegularExpression::CaseInsensitiveOption);

  static const QRegularExpression replacePattern(
      QStringLiteral(
          R"(\b(replace|rewrite|update|expand|populate|fill|complete|develop|improve|revise)\b)"),
      QRegularExpression::CaseInsensitiveOption);

  if (deletePattern.match(request).hasMatch()) {
    return RequestedOperation::Delete;
  }

  if (replaceScopePattern.match(request).hasMatch()) {
    return RequestedOperation::ReplaceScope;
  }

  if (insertPattern.match(request).hasMatch()) {
    return RequestedOperation::Insert;
  }

  if (replacePattern.match(request).hasMatch()) {
    return RequestedOperation::Replace;
  }

  return RequestedOperation::Unknown;
}

QString operationName(RequestedOperation operation) {
  switch (operation) {
  case RequestedOperation::Insert:
    return QStringLiteral("insert");

  case RequestedOperation::Replace:
    return QStringLiteral("replace");

  case RequestedOperation::ReplaceScope:
    return QStringLiteral("replace_scope");

  case RequestedOperation::Delete:
    return QStringLiteral("delete");

  case RequestedOperation::Unknown:
    return QStringLiteral("unknown");
  }

  return {};
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

  for (const EditCommand &command : commands) {
    array.append(commandToJson(command));
  }

  return QString::fromUtf8(
      QJsonDocument(array).toJson(QJsonDocument::Indented));
}

bool findExistsInScope(const EditCommand &command, const SectionInfo &section,
                       const QString &documentText) {
  if (command.findString.isEmpty()) {
    return false;
  }

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

} // namespace

EditPlanner::EditPlanner(InferenceService *inferenceService, QObject *parent)
    : QObject(parent), m_inferenceService(inferenceService) {
  if (!m_inferenceService) {
    return;
  }

  connect(m_inferenceService, &InferenceService::llmDelta, this,
          &EditPlanner::onLlmDelta);

  connect(m_inferenceService, &InferenceService::llmFinished, this,
          &EditPlanner::onLlmFinished);

  connect(m_inferenceService, &InferenceService::llmError, this,
          &EditPlanner::onLlmError);
}

void EditPlanner::start(TextEdit *editor, const QString &userRequest) {
  start(editor, userRequest, ScopeMode::Scoped);
}

void EditPlanner::start(TextEdit *editor, const QString &userRequest,
                        ScopeMode mode) {
  if (!m_inferenceService) {
    emit failed(QStringLiteral("Inference service is unavailable."));
    return;
  }

  if (!editor) {
    emit failed(QStringLiteral("No active document."));
    return;
  }

  auto *document = qobject_cast<TextDocument *>(editor->document());

  if (!document) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
    return;
  }

  if (m_active) {
    abort();
  }

  m_editor = editor;
  m_userRequest = userRequest.trimmed();
  m_streamingResponse.clear();
  m_scopeMode = mode;

  document->rebuildStructure();

  const QString documentText = editor->toPlainText();

  const QVector<SectionInfo> sections =
      collectSections(document->structure(), documentText);

  const QVector<const SectionInfo *> targets =
      resolveRequestedSections(m_userRequest, sections);

  const RequestedOperation requestedOp = requestedOperation(m_userRequest);

  const QStringList scopeIds = document->structure().scopeIds();

  QStringList targetLines;

  for (const SectionInfo *section : targets) {
    targetLines.append(QStringLiteral("\"%1\" -> \"%2\"")
                           .arg(section->title, section->scopeId));
  }

  const QString selectedText = editor->textCursor().selectedText();

  const QString selectionContext =
      selectedText.isEmpty()
          ? QString()
          : QStringLiteral("\nCurrent selection:\n%1\n").arg(selectedText);

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
            "The application has already determined the user's target "
            "section(s) and requested operation.\n"
            "You MUST follow those decisions exactly.\n"
            "\n"
            "Resolved target(s):\n"
            "%1\n"
            "\n"
            "Resolved operation:\n"
            "%2\n"
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
            "operation must be one of: insert, replace, delete, replace_scope.\n"
            "\n"
            "For insert:\n"
            "- find MUST be empty.\n"
            "- position MUST be before or after.\n"
            "\n"
            "For replace:\n"
            "- find MUST be the exact substring the user wants replaced. "
            "It must be a verbatim substring of the scope's current text.\n"
            "- find MUST be as short as possible while still being unique "
            "within the scope.\n"
            "- instruction MUST describe only what the replacement of that "
            "substring should look like. It must not describe a change to "
            "the surrounding sentence or paragraph.\n"
            "- position MUST be before or after.\n"
            "\n"
            "For delete:\n"
            "- find MUST be the exact substring to delete.\n"
            "- position MUST be before or after.\n"
            "\n"
            "For replace_scope:\n"
            "- Use this only when the user asks to rewrite or replace the "
            "ENTIRE body of a section or scope.\n"
            "- find MUST be empty.\n"
            "- position MUST be inside.\n"
            "- all MUST be false.\n"
            "- Do NOT use replace_scope when only part of the content "
            "changes; use replace with a find string instead.\n"
            "\n"
            "all MUST be false unless the user explicitly requests all, "
            "every, or each occurrence.\n"
            "\n"
            "instruction must describe exactly what this single edit "
            "should accomplish, including all requested details.\n"
            "\n"
            "Scope hierarchy:\n"
            "%3\n"
            "\n"
            "Document structure:\n"
            "%4\n"
            "\n"
            "User request:\n"
            "%5\n"
            "%6\n"
            "Document:\n"
            "%7")
            .arg(targetLines.join('\n'), operationName(requestedOp),
                 hierarchyMap(sections),
                 document->structure().sectionIndexForModel(), m_userRequest,
                 selectionContext, documentText);
  }

  m_payloadLogger.log(
      QStringLiteral("EDIT_PLAN_REQUEST"),
      QStringLiteral("Mode: %1\n"
                     "User Request: \"%2\"\n"
                     "Resolved Operation: %3\n"
                     "Resolved Targets (%4): %5\n"
                     "Doc Size: %6 chars | Prompt Size: %7 chars")
          .arg(m_scopeMode == ScopeMode::WholeFile ? QStringLiteral("whole")
                                                    : QStringLiteral("scoped"),
               m_userRequest, operationName(requestedOp),
               QString::number(targets.size()),
               targetLines.isEmpty() ? QStringLiteral("None")
                                     : targetLines.join(QStringLiteral(", ")),
               QString::number(documentText.size()),
               QString::number(prompt.size())));

  QJsonArray messages;

  messages.append(
      QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                  {QStringLiteral("content"), prompt}});

  m_active = true;

  QStringList contextScopeIds;

  if (m_scopeMode == ScopeMode::WholeFile) {
    if (!document->structure().root().id.isEmpty()) {
      contextScopeIds.append(document->structure().root().id);
    }
  } else {
    for (const SectionInfo *section : targets) {
      contextScopeIds.append(section->scopeId);
    }
  }

  emit contextScopes(contextScopeIds);

  m_inferenceService->sendChatRequest(messages, QString(), 0.7, 120000,
                                      EditGrammar::gbnf(scopeIds));
}

void EditPlanner::abort() {
  m_active = false;
  m_streamingResponse.clear();

  if (m_inferenceService) {
    m_inferenceService->abortChatRequest();
  }
}

void EditPlanner::onLlmDelta(const QString &text) {
  if (!m_active || text.isEmpty()) {
    return;
  }

  m_streamingResponse += text;

  processStream();
}

void EditPlanner::onLlmFinished() {
  if (!m_active) {
    return;
  }

  m_active = false;
  m_streamingResponse.clear();

  emit failed(QStringLiteral("Edit planner did not produce a complete plan."));
}

void EditPlanner::onLlmError(const QString &error) {
  if (!m_active) {
    return;
  }

  m_active = false;
  m_streamingResponse.clear();

  emit failed(QStringLiteral("LLM error: %1").arg(error));
}

bool EditPlanner::takeCompleteJsonValue(QString &buffer, QString &jsonText) {
  jsonText.clear();

  while (!buffer.isEmpty() && buffer.at(0).isSpace()) {
    buffer.remove(0, 1);
  }

  if (buffer.isEmpty()) {
    return false;
  }

  int start = 0;

  if (buffer.at(0) != QChar('{') && buffer.at(0) != QChar('[')) {
    const int objectStart = buffer.indexOf(QChar('{'));

    const int arrayStart = buffer.indexOf(QChar('['));

    if (objectStart < 0 && arrayStart < 0) {
      return false;
    }

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

      if (ch == QChar('"')) {
        inString = false;
      }

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
  if (!m_active) {
    return;
  }

  QString jsonText;

  if (!takeCompleteJsonValue(m_streamingResponse, jsonText)) {
    return;
  }

  m_active = false;

  if (m_inferenceService) {
    m_inferenceService->abortChatRequest();
  }

  QJsonParseError parseError;

  const QJsonDocument json =
      QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !json.isArray()) {
    m_payloadLogger.log(QStringLiteral("EDIT_PLAN_PARSE_ERROR"),
                        QStringLiteral("Error: %1 | Received JSON: %2")
                            .arg(parseError.errorString(), jsonText));

    emit failed(
        QStringLiteral("Invalid edit plan: %1").arg(parseError.errorString()));

    m_streamingResponse.clear();
    return;
  }

  if (!m_editor) {
    emit failed(QStringLiteral("No active editor."));
    return;
  }

  auto *textDocument = qobject_cast<TextDocument *>(m_editor->document());

  if (!textDocument) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
    return;
  }

  textDocument->rebuildStructure();

  const DocumentStructure &structure = textDocument->structure();

  const QString documentText = m_editor->toPlainText();

  const QVector<SectionInfo> sections =
      collectSections(structure, documentText);

  const QVector<const SectionInfo *> targets =
      resolveRequestedSections(m_userRequest, sections);

  const RequestedOperation expectedOperation =
      requestedOperation(m_userRequest);

  const QJsonArray items = json.array();

  QVector<EditCommand> edits;
  edits.reserve(items.size());

  for (int i = 0; i < items.size(); ++i) {
    const QJsonObject object = items.at(i).toObject();

    if (object.isEmpty()) {
      emit failed(
          QStringLiteral("Edit plan item %1 is not an object.").arg(i + 1));
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
      emit failed(
          QStringLiteral("Edit plan item %1 is missing a required field.")
              .arg(i + 1));
      return;
    }

    const QString operation = operationValue.toString();

    const QString scopeId = scopeValue.toString().trimmed();

    const QString position = positionValue.toString();

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
      emit failed(QStringLiteral("Edit plan item %1 has an invalid operation.")
                      .arg(i + 1));
      return;
    }

    if (position == QStringLiteral("before")) {
      command.position = EditCommand::Position::Before;
    } else if (position == QStringLiteral("after")) {
      command.position = EditCommand::Position::After;
    } else if (position == QStringLiteral("inside")) {
      command.position = EditCommand::Position::Inside;
    } else {
      emit failed(QStringLiteral("Edit plan item %1 has an invalid position.")
                      .arg(i + 1));
      return;
    }

    command.scopeId = scopeId;

    command.findString = findValue.toString();

    command.replaceAll = allValue.toBool();

    command.instruction = instructionValue.toString().trimmed();

    if (m_scopeMode == ScopeMode::WholeFile) {
      if (command.operation != EditCommand::Operation::ReplaceScope) {
        emit failed(QStringLiteral(
            "Whole-file mode requires operation 'replace_scope'."));
        return;
      }

      if (command.scopeId != QStringLiteral("document")) {
        emit failed(QStringLiteral(
            "Whole-file mode requires scope 'document'."));
        return;
      }

      if (command.position != EditCommand::Position::Inside) {
        emit failed(QStringLiteral(
            "Whole-file mode requires position 'inside'."));
        return;
      }

      if (!command.findString.isEmpty()) {
        emit failed(QStringLiteral(
            "Whole-file mode requires an empty find string."));
        return;
      }

      if (command.replaceAll) {
        emit failed(QStringLiteral(
            "Whole-file mode requires all=false."));
        return;
      }
    }

    const bool isDocumentRoot = command.scopeId == QStringLiteral("document");

    const SectionInfo *section =
        isDocumentRoot ? nullptr : findSection(sections, command.scopeId);

    if (!isDocumentRoot && !section && command.scopeId.isEmpty()) {
      if (command.operation != EditCommand::Operation::Insert) {
        emit failed(QStringLiteral("Edit plan item %1 uses an empty scope but "
                                   "is not an insertion.")
                        .arg(i + 1));
        return;
      }

      if (!documentText.isEmpty()) {
        emit failed(QStringLiteral("Edit plan item %1 uses an empty scope but "
                                   "the document is not empty.")
                        .arg(i + 1));
        return;
      }
    } else if (!isDocumentRoot && !section) {
      emit failed(
          QStringLiteral("Edit plan item %1 contains an invalid scope ID: %2")
              .arg(i + 1)
              .arg(command.scopeId));
      return;
    }

    if (m_scopeMode == ScopeMode::Scoped) {
      if (expectedOperation != RequestedOperation::Unknown &&
          operationName(expectedOperation) != operationName(command) &&
          !(expectedOperation == RequestedOperation::Replace &&
            command.operation == EditCommand::Operation::ReplaceScope)) {
        emit failed(
            QStringLiteral("The edit planner generated '%1', but "
                           "the user's request requires '%2'.")
                .arg(operationName(command), operationName(expectedOperation)));
        return;
      }
    }

    if (command.replaceAll && !explicitlyRequestsAll(m_userRequest)) {
      emit failed(
          QStringLiteral("Edit plan item %1 uses all=true without an explicit "
                         "request to affect all occurrences.")
              .arg(i + 1));
      return;
    }

    if (m_scopeMode == ScopeMode::Scoped && !targets.isEmpty()) {
      bool validTarget = false;

      for (const SectionInfo *target : targets) {
        if (scopeIdsEquivalent(target->scopeId, command.scopeId, sections)) {
          validTarget = true;
          break;
        }
      }

      if (!validTarget) {
        emit failed(
            QStringLiteral("Edit plan item %1 targets a section that was "
                           "not requested by the user.")
                .arg(i + 1));
        return;
      }
    }

    if (command.operation == EditCommand::Operation::Insert) {
      if (!command.findString.isEmpty()) {
        emit failed(QStringLiteral("Edit plan item %1 is an insert but has a "
                                   "non-empty find string.")
                        .arg(i + 1));
        return;
      }

      if (command.replaceAll) {
        emit failed(
            QStringLiteral("Edit plan item %1 is an insert with all=true.")
                .arg(i + 1));
        return;
      }
    } else if (command.operation == EditCommand::Operation::ReplaceScope) {
      if (!command.findString.isEmpty()) {
        emit failed(
            QStringLiteral("Edit plan item %1 is a replace_scope but has a "
                           "non-empty find string.")
                .arg(i + 1));
        return;
      }

      if (command.position != EditCommand::Position::Inside) {
        emit failed(
            QStringLiteral("Edit plan item %1 is a replace_scope but its "
                           "position is not inside.")
                .arg(i + 1));
        return;
      }

      if (command.replaceAll) {
        emit failed(
            QStringLiteral("Edit plan item %1 is a replace_scope with "
                           "all=true.")
                .arg(i + 1));
        return;
      }
    } else {
      if (command.findString.trimmed().isEmpty()) {
        emit failed(
            QStringLiteral("Edit plan item %1 requires find text.").arg(i + 1));
        return;
      }

      if (!isDocumentRoot &&
          (!section || !findExistsInScope(command, *section, documentText))) {
        emit failed(QStringLiteral("Edit plan item %1 uses find text that does "
                                   "not exist in the selected scope.")
                        .arg(i + 1));
        return;
      }
    }

    if (command.instruction.isEmpty()) {
      emit failed(QStringLiteral("Edit plan item %1 has an empty instruction.")
                      .arg(i + 1));
      return;
    }

    if (!command.isCommandValid()) {
      emit failed(
          QStringLiteral("Edit plan item %1 violates the edit protocol.")
              .arg(i + 1));
      return;
    }

    edits.append(command);
  }

  if (m_scopeMode == ScopeMode::WholeFile) {
    if (edits.size() != 1) {
      emit failed(QStringLiteral(
          "Whole-file mode requires exactly one edit."));
      return;
    }
  } else if (!targets.isEmpty()) {
    if (edits.size() != targets.size()) {
      emit failed(
          QStringLiteral(
              "The planner produced %1 edit(s) for %2 requested target(s).")
              .arg(edits.size(), targets.size()));
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
        emit failed(QStringLiteral(
            "The planner produced multiple edits for the same section."));
        return;
      }

      seenScopes.insert(key);
    }

    for (const SectionInfo *target : targets) {
      bool covered = false;

      for (const QString &seen : seenScopes) {
        if (scopeIdsEquivalent(target->scopeId, seen, sections)) {
          covered = true;
          break;
        }
      }

      if (!covered) {
        emit failed(QStringLiteral(
            "The planner did not produce an edit for a requested section."));
        return;
      }
    }
  }

  m_payloadLogger.log(QStringLiteral("EDIT_PLAN_VALIDATED"),
                      commandsToJson(edits));

  m_streamingResponse.clear();

  if (edits.isEmpty()) {
    emit failed(QStringLiteral("No edits required."));
    return;
  }

  QStringList referencedScopes;
  QSet<QString> seenReferenced;

  for (const EditCommand &edit : edits) {
    if (edit.scopeId.isEmpty()) {
      continue;
    }

    if (seenReferenced.contains(edit.scopeId)) {
      continue;
    }

    seenReferenced.insert(edit.scopeId);
    referencedScopes.append(edit.scopeId);
  }

  emit contextScopes(referencedScopes);

  QMetaObject::invokeMethod(
      this, [this, edits]() { emit planValidated(edits); },
      Qt::QueuedConnection);
}