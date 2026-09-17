#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUuid>

struct TranscriptEvent {
  enum class Type {
    UserMessage,
    AssistantMessage,
    ToolCall,
    ToolResult,
    MemoryProposal,
    EditPlan,
    Stage,
    Promotion,
    Error,
    Notice,
  };

  QUuid id;
  quint64 sequence = 0;
  Type type = Type::Notice;
  QDateTime timestamp;

  QString role;
  QString body;

  // Tool-related fields.
  QString toolName;
  QString toolCategory;
  QJsonObject toolArguments;
  QString toolResult;
  bool toolOk = true;
  qint64 toolDurationMs = 0;

  // Proposal-related fields.
  QString proposalKey;
  QString proposalFact;
  QString proposalRationale;
  QString proposalStatus;   // "pending", "accepted", "rejected"
  QString proposalScope;    // "global" or "session"
  QString proposalAcceptedScope; // scope actually accepted into
  // The assistant message text that was emitted alongside this proposal.
  // Rendered above the fact in the proposal card.
  QString proposalContext;

  // Edit plan fields.
  QString planId;
  QString planFilePath;
  QString planInstruction;
  QJsonArray planCommands;
  QString planStatus;       // "pending", "applying", "applied",
  // "cancelled", "failed"
  QString planResult;

  // Stage / promotion.
  QString filePath;

  int inputTokens = -1;
  int outputTokens = -1;

  static QString typeToString(Type type);
  static Type typeFromString(const QString &value);

  static QString categoryToString(const QString &raw);
};