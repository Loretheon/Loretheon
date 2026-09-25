#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

class ChatNode {
public:
  enum class Kind {
    UserText,
    AssistantText,
    JobSearch,
    JobDelegate,
    JobPromote,
    JobRead,
    JobEdit,
    Tool,
    Status,
    Error,
  };

  enum class State {
    None,
    Pending,
    Running,
    Done,
    Failed,
    Cancelled,
    Waiting,
  };

  ChatNode() = default;
  ChatNode(Kind kind, const QString &text);

  QString id;
  QString parentId;

  Kind kind = Kind::AssistantText;
  State state = State::None;

  QString text;
  QString detail;

  QString jobId;
  QString result;
  QString error;

  QDateTime createdAt;
  QDateTime updatedAt;

  QVector<QString> children;

  bool collapsed = false;

  bool isJob() const {
    return kind == Kind::JobSearch || kind == Kind::JobDelegate ||
           kind == Kind::JobPromote || kind == Kind::JobRead ||
           kind == Kind::JobEdit;
  }

  bool isText() const {
    return kind == Kind::UserText || kind == Kind::AssistantText;
  }

  bool isTerminal() const {
    return state == State::Done || state == State::Failed ||
           state == State::Cancelled;
  }

  static bool defaultCollapsed(Kind kind);
};