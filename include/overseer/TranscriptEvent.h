#pragma once

#include <QJsonObject>
#include <QString>
#include <QDateTime>
#include <QUuid>

struct TranscriptEvent {
  enum class Type {
    UserMessage,
    AssistantMessage,
    ToolCall,
    ToolResult,
    MemoryProposal,
    Stage,
    Promotion,
    Error,
    Notice,
  };

  QUuid id;
  quint64 sequence = 0;
  Type type = Type::Notice;
  QDateTime timestamp;

  QString role;         // "user", "assistant", "tool", "system", "error"
  QString body;         // textual content

  // Tool-related fields, empty for other types.
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
  QString proposalStatus;

  // Stage / promotion.
  QString filePath;

  // Token counts, if known.
  int inputTokens = -1;
  int outputTokens = -1;

  static QString typeToString(Type type);
  static Type typeFromString(const QString &value);

  static QString categoryToString(const QString &raw);
};