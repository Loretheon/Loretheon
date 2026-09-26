#include "EditSession.h"

#include "EditApplier.h"
#include "EditCandidateView.h"

#include "inference/InferenceService.h"
#include "PayloadLogger.h"
#include "TextEdit.h"
#include "TextDocument.h"

#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace {

constexpr int InvalidRevision = -1;
constexpr int ShortFragmentThreshold = 120;

QString noActiveEditorError() { return QStringLiteral("No active editor."); }

QString invalidDocumentError() {
  return QStringLiteral("Active editor does not use TextDocument.");
}

QString sanitizeGeneratedText(const QString &raw, bool isShortFragment) {
  QString text = raw;

  static const QRegularExpression fenceRe(
      QStringLiteral("^\\s*```[a-zA-Z0-9_-]*\\s*\\n?"),
      QRegularExpression::MultilineOption);

  static const QRegularExpression fenceEndRe(
      QStringLiteral("\\n?\\s*```\\s*$"),
      QRegularExpression::MultilineOption);

  text.remove(fenceRe);
  text.remove(fenceEndRe);

  if (isShortFragment) {
    text = text.trimmed();

    static const QRegularExpression dashWrap(
        QStringLiteral("^\\s*---+\\s*(.*?)\\s*---+\\s*$"),
        QRegularExpression::DotMatchesEverythingOption);

    const auto m = dashWrap.match(text);
    if (m.hasMatch())
      text = m.captured(1).trimmed();

    if (text.size() >= 2 && text.startsWith(QChar('"')) &&
        text.endsWith(QChar('"')))
      text = text.mid(1, text.size() - 2);

    if (text.contains(QChar('\n'))) {
      const QStringList lines = text.split(QChar('\n'));
      for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
          text = trimmed;
          break;
        }
      }
    }
  } else {
    while (text.endsWith(QChar('\n')) || text.endsWith(QChar('\r')))
      text.chop(1);

    text = text.trimmed();
  }

  return text;
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

EditSession::EditSession(TextEdit *editor, QObject *parent)
    : QObject(parent), m_editor(editor), m_applier(new EditApplier(this)),
      m_candidateView(new EditCandidateView()),
      m_historyModel(new HistoryModel(this)),
      m_payloadLogger(new PayloadLogger(this)) {
  connect(m_candidateView, &EditCandidateView::candidateSelected, this,
          &EditSession::onCandidateSelected);

  connect(m_applier, &EditApplier::applied, this, &EditSession::applied);

  connect(m_applier, &EditApplier::failed, this, &EditSession::failed);
}

void EditSession::setEditor(TextEdit *editor) {
  if (m_editor == editor)
    return;

  abort();
  m_editor = editor;
}

void EditSession::setInferenceService(InferenceService *service) {
  if (m_inferenceService == service)
    return;

  if (m_inferenceService)
    disconnect(m_inferenceService, nullptr, this, nullptr);

  m_inferenceService = service;

  if (!m_inferenceService)
    return;

  connect(m_inferenceService, &InferenceService::llmDelta, this,
          [this](const InferenceService::RequestToken &token,
                 const QString &text) {
            const auto it = m_tokenToEditId.constFind(token);

            if (it == m_tokenToEditId.constEnd())
              return;

            const int editId = it.value();

            m_generationBuffers[editId] += text;

            PendingEdit *edit = findPendingEdit(editId);

            if (!edit)
              return;

            edit->generatedText = m_generationBuffers.value(editId);

            emit pendingEditUpdated(*edit);
          });

  connect(m_inferenceService, &InferenceService::llmFinished, this,
          [this](const InferenceService::RequestToken &token) {
            const auto it = m_tokenToEditId.constFind(token);

            if (it == m_tokenToEditId.constEnd())
              return;

            const int editId = it.value();

            m_tokenToEditId.remove(token);
            m_generationTokens.remove(editId);

            PendingEdit *edit = findPendingEdit(editId);

            if (edit) {
              const QString raw = m_generationBuffers.value(editId);

              if (m_payloadLogger) {
                m_payloadLogger->log(
                    PayloadLogger::Subsystem::Edit,
                    QStringLiteral("GENERATION_RESPONSE"),
                    QStringLiteral("Edit: %1\n\nResponse:\n%2")
                        .arg(editId)
                        .arg(raw));
              }

              const bool isShortFragment =
                  edit->command.operation == EditCommand::Operation::Replace &&
                  edit->command.findString.size() < ShortFragmentThreshold;

              const QString sanitized =
                  sanitizeGeneratedText(raw, isShortFragment);

              if (!sanitized.isEmpty()) {
                edit->command.newString = sanitized;
                edit->completed = true;

                emit pendingEditUpdated(*edit);
                emit pendingEditFinished(*edit);
              } else {
                emit failed(QStringLiteral(
                    "Edit %1 produced no usable content.").arg(editId));
              }
            }

            m_generationBuffers.remove(editId);

            if (allPendingEditsCompleted()) {
              setState(State::WaitingForReview);
              emit generationFinished(true);
              emit reviewReady();
            }
          });

  connect(m_inferenceService, &InferenceService::llmError, this,
          [this](const InferenceService::RequestToken &token,
                 const QString &error) {
            const auto it = m_tokenToEditId.constFind(token);

            if (it == m_tokenToEditId.constEnd())
              return;

            const int editId = it.value();

            m_tokenToEditId.remove(token);
            m_generationTokens.remove(editId);
            m_generationBuffers.remove(editId);

            if (m_payloadLogger) {
              m_payloadLogger->log(
                  PayloadLogger::Subsystem::Edit,
                  QStringLiteral("GENERATION_ERROR"),
                  QStringLiteral("Edit: %1\nError:\n%2")
                      .arg(editId)
                      .arg(error));
            }

            emit failed(QStringLiteral("Edit %1 generation failed: %2")
                            .arg(editId)
                            .arg(error));

            if (allPendingEditsCompleted()) {
              setState(State::WaitingForReview);
              emit generationFinished(true);
              emit reviewReady();
            }
          });
}
bool EditSession::hasConflicts() const {
  return std::any_of(m_pendingEdits.cbegin(), m_pendingEdits.cend(),
                     [](const PendingEdit &edit) { return edit.hasConflict; });
}

void EditSession::detectConflicts() {
  for (PendingEdit &edit : m_pendingEdits)
    edit.hasConflict = false;

  for (int first = 0; first < m_pendingEdits.size(); ++first) {
    const EditMatch &firstMatch = m_pendingEdits.at(first).match;

    if (!firstMatch.isValid())
      continue;

    for (int second = first + 1; second < m_pendingEdits.size(); ++second) {
      const EditMatch &secondMatch = m_pendingEdits.at(second).match;

      if (!secondMatch.isValid())
        continue;

      const bool overlaps = firstMatch.start < secondMatch.end &&
                            firstMatch.end > secondMatch.start;

      if (!overlaps)
        continue;

      m_pendingEdits[first].hasConflict = true;
      m_pendingEdits[second].hasConflict = true;
    }
  }
}

bool EditSession::validatePlan(const QJsonArray &planArray) {
  abort();

  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  setState(State::ValidatingPlan);

  QVector<EditCommand> commands;
  commands.reserve(planArray.size());

  for (int i = 0; i < planArray.size(); ++i) {
    const QJsonValue value = planArray.at(i);

    if (!value.isObject()) {
      setState(State::Idle);
      emit failed(
          QStringLiteral("Plan item %1 is not an object.").arg(i + 1));
      return false;
    }

    const EditCommand command = EditCommand::fromJson(value.toObject());

    if (!command.isCommandValid()) {
      setState(State::Idle);
      emit failed(
          QStringLiteral("Plan item %1 is not a valid edit command.")
              .arg(i + 1));
      return false;
    }

    commands.append(command);
  }

  if (commands.isEmpty()) {
    setState(State::Idle);
    emit failed(QStringLiteral("Plan contains no valid edit commands."));
    return false;
  }

  document->rebuildStructure();

  const DocumentStructure &structure = document->structure();

  for (int i = 0; i < commands.size(); ++i) {
    const EditCommand &command = commands.at(i);

    if (command.scopeId.isEmpty()) {
      if (command.operation != EditCommand::Operation::Insert) {
        setState(State::Idle);
        emit failed(
            QStringLiteral("Plan item %1 uses an empty scope but is not an "
                           "insertion.")
                .arg(i + 1));
        return false;
      }

      continue;
    }

    if (!structure.find(command.scopeId)) {
      setState(State::Idle);
      emit failed(QStringLiteral("Plan item %1 references unknown scope '%2'.")
                      .arg(i + 1)
                      .arg(command.scopeId));
      return false;
    }
  }

  m_documentRevision = document->revision();

  setState(State::AwaitingPlanApproval);

  emit planValidated(commands);
  return true;
}

bool EditSession::executePlan(const QVector<EditCommand> &commands) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  if (commands.isEmpty()) {
    emit failed(QStringLiteral("Approved plan is empty."));
    return false;
  }

  document->rebuildStructure();

  if (m_documentRevision != InvalidRevision &&
      document->revision() != m_documentRevision) {
    setState(State::Idle);
    emit failed(QStringLiteral(
        "Document changed after plan validation. Please re-plan."));
    return false;
  }

  m_pendingEdits.clear();
  m_generationTokens.clear();
  m_tokenToEditId.clear();
  m_generationBuffers.clear();

  setState(State::Matching);

  m_documentRevision = document->revision();

  for (int index = 0; index < commands.size(); ++index) {
    const EditCommand &command = commands.at(index);

    EditMatch match;

    if (!resolveCommand(command, match)) {
      setState(State::Idle);
      return false;
    }

    PendingEdit edit;
    edit.id = index + 1;
    edit.command = command;
    edit.match = match;
    edit.accepted = true;
    edit.completed = false;

    m_pendingEdits.append(edit);
  }

  detectConflicts();

  if (hasConflicts())
    emit conflictsDetected();

  setState(State::WaitingForReview);
  emit planReady(commands);
  emit pendingEditsChanged();

  return true;
}

bool EditSession::resolvePlan(const QJsonArray &planArray) {
  if (!validatePlan(planArray))
    return false;

  QVector<EditCommand> commands;
  commands.reserve(planArray.size());

  for (const QJsonValue &value : planArray) {
    if (!value.isObject())
      continue;

    const EditCommand command = EditCommand::fromJson(value.toObject());

    if (command.isCommandValid())
      commands.append(command);
  }

  return executePlan(commands);
}

bool EditSession::propose(const EditCommand &command) {
  abort();

  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  if (!command.isCommandValid()) {
    emit failed(QStringLiteral("Invalid edit command."));
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  setState(State::Matching);

  document->rebuildStructure();
  m_documentRevision = document->revision();

  m_pendingCommand = command;
  m_pendingCandidates.clear();

  EditMatcher::Result result;

  if (command.operation == EditCommand::Operation::Insert) {
    EditMatch match;

    if (!createInsertionMatch(command, match)) {
      setState(State::Idle);
      return false;
    }

    result.candidates.append(match);
  } else {
    result = m_matcher.find(*document, command);
  }

  if (result.candidates.isEmpty()) {
    setState(State::Idle);
    emit failed(QStringLiteral("No matching text found in scope '%1'.")
                    .arg(command.scopeId));
    return false;
  }

  m_pendingCandidates = result.candidates;

  if (m_pendingCandidates.size() == 1) {
    applyCandidate(m_pendingCandidates.first());
    return true;
  }

  setState(State::AwaitingSelection);
  emit candidatesReady(m_pendingCandidates);
  return true;
}

bool EditSession::proposeMany(const QVector<EditCommand> &commands) {
  abort();

  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  if (commands.isEmpty()) {
    emit failed(QStringLiteral("Edit batch is empty."));
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  for (int index = 0; index < commands.size(); ++index) {
    if (commands.at(index).isValid())
      continue;

    emit failed(QStringLiteral("Edit %1 is invalid.").arg(index + 1));
    return false;
  }

  setState(State::Matching);

  document->rebuildStructure();
  m_documentRevision = document->revision();

  QVector<EditMatch> matches;

  if (!resolveBatch(commands, matches)) {
    setState(State::Idle);
    return false;
  }

  setState(State::Applying);

  const bool applied = m_applier->applyBatch(*document, commands, matches);

  m_pendingCandidates.clear();
  setState(State::Idle);

  return applied;
}

bool EditSession::prepareStreaming(const EditCommand &command,
                                   int pendingEditId) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  if (!command.isCommandValid()) {
    emit failed(QStringLiteral("Invalid structural edit command."));
    return false;
  }

  if (m_applier && m_applier->isStreaming())
    m_applier->cancelStreaming();

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  document->rebuildStructure();
  m_documentRevision = document->revision();

  m_pendingCommand = command;
  m_pendingCandidates.clear();

  setState(State::Matching);

  EditMatch match;

  if (!resolveCommand(command, match)) {
    setState(State::Idle);
    return false;
  }

  if (document->revision() != m_documentRevision) {
    setState(State::Idle);
    emit failed(QStringLiteral("Document changed before streaming started."));
    return false;
  }

  m_pendingMatch = match;

  PendingEdit *pendingEdit = findPendingEdit(pendingEditId);

  if (!pendingEdit) {
    PendingEdit edit;
    edit.id = pendingEditId;
    edit.command = command;
    edit.match = match;
    edit.generatedText.clear();
    edit.accepted = true;
    edit.completed = false;

    m_pendingEdits.append(edit);
    pendingEdit = &m_pendingEdits.last();
  } else {
    pendingEdit->command = command;
    pendingEdit->match = match;
    pendingEdit->generatedText.clear();
    pendingEdit->accepted = true;
    pendingEdit->completed = false;
    pendingEdit->hasConflict = false;
  }

  emit pendingEditStarted(*pendingEdit);

  return true;
}

void EditSession::startAllPendingEdits() {
  if (m_pendingEdits.isEmpty()) {
    emit generationFinished(true);
    return;
  }

  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.completed)
      continue;

    if (edit.command.operation == EditCommand::Operation::Insert) {
      completeInsertFromInstruction(edit.id);
      continue;
    }

    if (edit.command.operation == EditCommand::Operation::Delete) {
      edit.completed = true;
      emit pendingEditFinished(edit);
      continue;
    }

    dispatchGeneration(edit);
  }

  if (allPendingEditsCompleted()) {
    setState(State::WaitingForReview);
    emit generationFinished(true);
    emit reviewReady();
  }
}

void EditSession::dispatchGeneration(PendingEdit &edit) {
  if (!m_inferenceService) {
    emit failed(QStringLiteral("No inference service available."));
    return;
  }

  if (m_generationTokens.contains(edit.id))
    return;

  if (!prepareStreaming(edit.command, edit.id))
    return;

  QJsonArray messages;

  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("system")},
      {QStringLiteral("content"), systemPromptFor(edit.command)}});

  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("user")},
      {QStringLiteral("content"), userPromptFor(edit.command)}});

  if (m_payloadLogger) {
    m_payloadLogger->log(
        PayloadLogger::Subsystem::Edit,
        QStringLiteral("GENERATION_REQUEST"),
        QStringLiteral("Edit: %1\nScope: %2\nOperation: %3\n\nMessages:\n%4")
            .arg(edit.id)
            .arg(edit.command.scopeId)
            .arg(static_cast<int>(edit.command.operation))
            .arg(formatMessagesForLog(messages)));
  }

  const InferenceService::RequestToken token =
    m_inferenceService->sendChatRequest(messages, QString(), 0.7, 120000,
                                        QString(), QJsonObject(),
                                        QJsonArray(), m_sessionId);

  m_generationTokens.insert(edit.id, token);
  m_tokenToEditId.insert(token, edit.id);
  m_generationBuffers.insert(edit.id, QString());
}

QString EditSession::systemPromptFor(const EditCommand &command) const {
  if (command.operation == EditCommand::Operation::Replace) {
    return QStringLiteral(
        "You are a text-substitution engine. The application will replace "
        "exactly one occurrence of a target substring in a document with "
        "your output.\n"
        "\n"
        "Rules:\n"
        "- Output ONLY the replacement for the target substring.\n"
        "- Output must be a single line. No newlines.\n"
        "- Do NOT include the target substring in your output.\n"
        "- Do NOT wrap your output in backticks, quotes, ---, or any "
        "other delimiter.\n"
        "- Do NOT explain your output.\n");
  }

  if (command.operation == EditCommand::Operation::ReplaceScope) {
    return QStringLiteral(
        "You are a text generator. The application will replace the "
        "entire body of a section or scope with your output.\n"
        "\n"
        "Rules:\n"
        "- Output ONLY the new body content for the scope.\n"
        "- Do NOT include the scope's heading or title line.\n"
        "- Do NOT wrap your output in backticks or fences.\n"
        "- Do NOT explain your output.\n"
        "- End your output with exactly one trailing newline.\n");
  }

  return QStringLiteral(
      "You are an automated text generator. Output only the requested "
      "content. No commentary. No markdown fences. No explanation.");
}

QString EditSession::userPromptFor(const EditCommand &command) const {
  if (command.operation == EditCommand::Operation::Replace) {
    return QStringLiteral(
               "Target substring:\n%1\n\nInstruction:\n%2\n\n"
               "Output the replacement now. Nothing else.")
        .arg(command.findString, command.instruction);
  }

  if (command.operation == EditCommand::Operation::ReplaceScope) {
    return QStringLiteral(
               "Target scope: %1\n\nInstruction:\n%2\n\n"
               "Output the new body now.")
        .arg(command.scopeId, command.instruction);
  }

  return QStringLiteral("Instruction:\n%1\n\nTarget scope: %2")
      .arg(command.instruction, command.scopeId);
}

bool EditSession::appendStreaming(const QString &text) {
  if (m_generationTokens.isEmpty()) {
    emit failed(QStringLiteral("No streaming edit is active."));
    return false;
  }

  if (text.isEmpty())
    return true;

  const int editId = m_tokenToEditId.constBegin().value();

  m_generationBuffers[editId] += text;

  return true;
}

bool EditSession::finishStreaming() {
  if (m_generationTokens.isEmpty()) {
    emit failed(QStringLiteral("No streaming edit is active."));
    return false;
  }

  return true;
}

bool EditSession::allPendingEditsCompleted() const {
  if (m_pendingEdits.isEmpty())
    return false;

  for (const PendingEdit &edit : m_pendingEdits) {
    if (!edit.completed)
      return false;
  }

  return true;
}

void EditSession::abort() {
  if (m_applier && m_applier->isStreaming())
    m_applier->cancelStreaming();

  for (auto it = m_generationTokens.constBegin();
       it != m_generationTokens.constEnd(); ++it) {
    if (m_inferenceService && !it.value().isNull())
      m_inferenceService->abortChatRequest(it.value());
  }

  const bool wasActive = m_state != State::Idle;

  m_pendingCandidates.clear();
  m_pendingEdits.clear();
  m_generationTokens.clear();
  m_tokenToEditId.clear();
  m_generationBuffers.clear();
  m_pendingMatch = EditMatch();
  m_pendingCommand = EditCommand();
  m_documentRevision = InvalidRevision;

  setState(State::Idle);

  if (wasActive)
    emit aborted();
}

void EditSession::setState(State state) {
  if (m_state == state)
    return;

  m_state = state;
  emit stateChanged(m_state);
}

void EditSession::applyCandidate(const EditMatch &match) {
  if (!m_editor) {
    setState(State::Idle);
    emit failed(noActiveEditorError());
    return;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    setState(State::Idle);
    emit failed(invalidDocumentError());
    return;
  }

  if (document->revision() != m_documentRevision) {
    m_pendingCandidates.clear();
    setState(State::Idle);
    emit failed(
        QStringLiteral("Document changed before the edit was applied."));
    return;
  }

  if (!match.isValid()) {
    setState(State::Idle);
    emit failed(QStringLiteral("Invalid edit match."));
    return;
  }

  setState(State::Applying);

  m_applier->apply(*document, m_pendingCommand, match);

  m_pendingCandidates.clear();
  setState(State::Idle);
}

bool EditSession::resolveCommand(const EditCommand &command, EditMatch &match) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  if (command.operation == EditCommand::Operation::Insert)
    return createInsertionMatch(command, match);

  if (command.operation == EditCommand::Operation::ReplaceScope) {
    const EditMatcher::Result result = m_matcher.find(*document, command);

    if (result.candidates.isEmpty()) {
      emit failed(QStringLiteral("Scope body for '%1' could not be located.")
                      .arg(command.scopeId));
      return false;
    }

    match = result.candidates.first();
    return true;
  }

  const EditMatcher::Result result = m_matcher.find(*document, command);

  if (result.candidates.isEmpty()) {
    emit failed(QStringLiteral("No matching text found in scope '%1'.")
                    .arg(command.scopeId));
    return false;
  }

  if (result.candidates.size() > 1 && !command.replaceAll) {
    emit failed(QStringLiteral("Edit has %1 possible matches in scope '%2'.")
                    .arg(result.candidates.size())
                    .arg(command.scopeId));
    return false;
  }

  match = result.candidates.first();
  return true;
}

bool EditSession::resolveSingle(const EditCommand &command, EditMatch &match) {
  return resolveCommand(command, match);
}

bool EditSession::resolveBatch(const QVector<EditCommand> &commands,
                               QVector<EditMatch> &matches) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  matches.clear();
  matches.reserve(commands.size());

  for (int index = 0; index < commands.size(); ++index) {
    EditMatch match;

    if (resolveSingle(commands.at(index), match)) {
      matches.append(match);
      continue;
    }

    emit failed(
        QStringLiteral("Edit %1 could not be resolved.").arg(index + 1));
    return false;
  }

  if (document->revision() != m_documentRevision) {
    emit failed(QStringLiteral("Document changed while resolving edits."));
    return false;
  }

  return true;
}

bool EditSession::createInsertionMatch(const EditCommand &command,
                                       EditMatch &match) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  document->rebuildStructure();

  const QString documentText = document->toPlainText();
  const int documentLength = documentText.size();

  const DocumentStructure &structure = document->structure();
  const DocumentNode *scope = nullptr;

  if (command.scopeId.isEmpty()) {
    if (!documentText.isEmpty()) {
      emit failed(QStringLiteral(
          "Empty insertion scope is only valid for an empty document."));
      return false;
    }
  } else {
    scope = structure.find(command.scopeId);

    if (!scope) {
      emit failed(QStringLiteral("Insertion scope '%1' was not found.")
                      .arg(command.scopeId));
      return false;
    }

    if (scope->start == scope->end && documentLength > 0) {
      emit failed(QStringLiteral(
          "Insertion scope '%1' has zero length and cannot anchor an "
          "insertion.").arg(command.scopeId));
      return false;
    }
  }

  const int scopeStart = scope ? scope->start : 0;
  const int scopeEnd = scope ? scope->end : documentLength;

  if (scopeStart < 0 || scopeStart > documentLength) {
    emit failed(QStringLiteral("Insertion scope '%1' has an invalid range.")
                    .arg(command.scopeId));
    return false;
  }

  if (scopeEnd < scopeStart || scopeEnd > documentLength) {
    emit failed(QStringLiteral("Insertion scope '%1' has an invalid range.")
                    .arg(command.scopeId));
    return false;
  }

  if (!command.findString.isEmpty()) {
    const QString scopeText =
        documentText.mid(scopeStart, scopeEnd - scopeStart);

    const int anchorOffset = scopeText.indexOf(command.findString);

    if (anchorOffset < 0) {
      emit failed(QStringLiteral(
          "Insertion anchor was not found in scope '%1'.")
                      .arg(command.scopeId));
      return false;
    }

    const int anchorStart = scopeStart + anchorOffset;
    const int anchorEnd = anchorStart + command.findString.size();

    const int insertionPosition =
        command.position == EditCommand::Position::Before ? anchorStart
                                                          : anchorEnd;

    match.start = insertionPosition;
    match.end = insertionPosition;
    match.highlightStart = scopeStart;
    match.highlightEnd = scopeEnd;
    match.editDistance = 0;
    match.matchedText = command.findString;
    return true;
  }

  const int insertionPosition =
      command.position == EditCommand::Position::After ? scopeEnd : scopeStart;

  match.start = insertionPosition;
  match.end = insertionPosition;
  match.highlightStart = scopeStart;
  match.highlightEnd = scopeEnd;
  match.editDistance = 0;
  match.matchedText.clear();

  return true;
}

bool EditSession::setPendingEditAccepted(int id, bool accepted) {
  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.id != id)
      continue;

    edit.accepted = accepted;

    emit pendingEditUpdated(edit);
    emit pendingEditsChanged();
    return true;
  }

  return false;
}

bool EditSession::acceptPendingEdit(int id) {
  return setPendingEditAccepted(id, true);
}

bool EditSession::rejectPendingEdit(int id) {
  return setPendingEditAccepted(id, false);
}

void EditSession::acceptAllPendingEdits() {
  for (PendingEdit &edit : m_pendingEdits)
    edit.accepted = true;

  emit pendingEditsChanged();
}

void EditSession::rejectAllPendingEdits() {
  for (PendingEdit &edit : m_pendingEdits)
    edit.accepted = false;

  emit pendingEditsChanged();
}

bool EditSession::applyPendingEdit(PendingEdit &edit) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  if (!edit.completed) {
    emit failed(
        QStringLiteral("Pending edit %1 is not complete.").arg(edit.id));
    return false;
  }

  if (!edit.accepted)
    return true;

  if (edit.command.operation != EditCommand::Operation::Delete &&
      edit.command.newString.isEmpty()) {
    emit failed(QStringLiteral("Pending edit %1 has no generated content.")
                    .arg(edit.id));
    return false;
  }

  return m_applier->apply(*document, edit.command, edit.match);
}

bool EditSession::applyAcceptedPendingEdits() {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  QVector<PendingEdit *> selected;

  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.completed && edit.accepted)
      selected.append(&edit);
  }

  std::sort(selected.begin(), selected.end(),
            [](const PendingEdit *left, const PendingEdit *right) {
              return left->match.start > right->match.start;
            });

  QSet<QString> touchedScopeIds;
  QVector<HistoryEntry> historyEntries;

  for (PendingEdit *edit : selected) {
    if (!edit->command.scopeId.isEmpty())
      touchedScopeIds.insert(edit->command.scopeId);

    if (!applyPendingEdit(*edit))
      continue;

    HistoryEntry entry;
    entry.userRequest = edit->command.instruction;
    entry.command = edit->command;
    entry.outcome = QStringLiteral("Applied");
    entry.detail = edit->generatedText.left(240);
    entry.timestampMs = QDateTime::currentMSecsSinceEpoch();

    historyEntries.append(entry);
  }

  document->rebuildStructure();

  m_pendingEdits.clear();

  emit pendingEditsChanged();
  setState(State::Idle);

  if (!touchedScopeIds.isEmpty()) {
    emit pendingEditsApplied(QStringList(touchedScopeIds.cbegin(),
                                         touchedScopeIds.cend()));
  }

  if (!historyEntries.isEmpty() && m_historyModel)
    m_historyModel->appendMany(historyEntries);

  return true;
}

void EditSession::clearPendingEdits() {
  m_pendingEdits.clear();
  emit pendingEditsChanged();

  if (m_state == State::WaitingForReview)
    setState(State::Idle);
}

PendingEdit *EditSession::findPendingEdit(int id) {
  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.id == id)
      return &edit;
  }

  return nullptr;
}

void EditSession::onCandidateSelected(int index) {
  if (m_state != State::AwaitingSelection)
    return;

  if (!m_editor) {
    abort();
    emit failed(noActiveEditorError());
    return;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    abort();
    emit failed(invalidDocumentError());
    return;
  }

  if (document->revision() != m_documentRevision) {
    m_pendingCandidates.clear();
    setState(State::Idle);
    emit failed(QStringLiteral("Document changed before candidate selection."));
    return;
  }

  if (index < 0 || index >= m_pendingCandidates.size()) {
    emit failed(QStringLiteral("Invalid edit candidate."));
    return;
  }

  applyCandidate(m_pendingCandidates.at(index));
}

bool EditSession::completeInsertFromInstruction(int pendingEditId) {
  PendingEdit *edit = findPendingEdit(pendingEditId);

  if (!edit) {
    emit failed(QStringLiteral("Pending edit %1 was not found.")
                    .arg(pendingEditId));
    return false;
  }

  if (edit->command.operation != EditCommand::Operation::Insert) {
    emit failed(QStringLiteral("Pending edit %1 is not an insert.")
                    .arg(pendingEditId));
    return false;
  }

  QString content = edit->command.instruction.trimmed();

  if (content.isEmpty()) {
    emit failed(QStringLiteral("Insert edit %1 has no instruction to insert.")
                    .arg(pendingEditId));
    return false;
  }

  static const QRegularExpression metaRe(
     QStringLiteral(
         R"(^\s*(?:Insert|Add|Append|Create|Write)\b[^\n]*?(?:content|following|below)\s*:\s*\n)"),
     QRegularExpression::CaseInsensitiveOption);

  const auto metaMatch = metaRe.match(content);

  if (metaMatch.hasMatch())
    content = content.mid(metaMatch.capturedLength()).trimmed();

  edit->command.newString = content;
  edit->generatedText = content;
  edit->completed = true;

  emit pendingEditUpdated(*edit);
  emit pendingEditFinished(*edit);
  emit pendingEditsChanged();
  emit reviewReady();

  return true;
}