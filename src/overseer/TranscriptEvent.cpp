#include "../../include/overseer/TranscriptEvent.h"

QString TranscriptEvent::typeToString(Type type) {
  switch (type) {
  case Type::UserMessage: return QStringLiteral("user");
  case Type::AssistantMessage: return QStringLiteral("assistant");
  case Type::ToolCall: return QStringLiteral("tool_call");
  case Type::ToolResult: return QStringLiteral("tool_result");
  case Type::MemoryProposal: return QStringLiteral("memory_proposal");
  case Type::Stage: return QStringLiteral("stage");
  case Type::Promotion: return QStringLiteral("promotion");
  case Type::Error: return QStringLiteral("error");
  case Type::Notice: return QStringLiteral("notice");
  }
  return QStringLiteral("notice");
}

TranscriptEvent::Type TranscriptEvent::typeFromString(const QString &value) {
  if (value == QStringLiteral("user")) return Type::UserMessage;
  if (value == QStringLiteral("assistant")) return Type::AssistantMessage;
  if (value == QStringLiteral("tool_call")) return Type::ToolCall;
  if (value == QStringLiteral("tool_result")) return Type::ToolResult;
  if (value == QStringLiteral("memory_proposal")) return Type::MemoryProposal;
  if (value == QStringLiteral("stage")) return Type::Stage;
  if (value == QStringLiteral("promotion")) return Type::Promotion;
  if (value == QStringLiteral("error")) return Type::Error;
  return Type::Notice;
}

QString TranscriptEvent::categoryToString(const QString &raw) {
  return raw.trimmed().toLower();
}