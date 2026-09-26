#include "../../include/assistant/ChatNode.h"

ChatNode::ChatNode(Kind kind, const QString &text) : kind(kind), text(text) {
  createdAt = QDateTime::currentDateTime();
  updatedAt = createdAt;
  collapsed = defaultCollapsed(kind);
}

bool ChatNode::defaultCollapsed(Kind kind) {
  switch (kind) {
  case Kind::AssistantText:
  case Kind::JobSearch:
  case Kind::JobDelegate:
  case Kind::JobPromote:
  case Kind::JobRead:
  case Kind::JobEdit:
  case Kind::Tool:
    return true;
  default:
    return false;
  }
}