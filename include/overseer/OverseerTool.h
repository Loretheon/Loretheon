#pragma once

#include <QJsonObject>
#include <QString>

#include <functional>

class OverseerTool {
public:
  struct Result {
    bool ok = false;
    QString output;
    QString error;
  };

  struct Context {
    QString sessionFolder;
    QString outputFolder;
    QString notesRoot;

    // Called by tools that want to open a review surface for an edit to a
    // referenced note. The tool copies the note into outputFolder first and
    // then invokes this callback with:
    //   copyPath      - the session-local copy that was created
    //   originalPath  - the original note path (read-only for the tool)
    //   instruction   - the natural-language edit instruction
    //
    // The callback may be empty; if so, the tool should report failure to
    // the LLM (there is nowhere to send the request).
    std::function<void(const QString &copyPath,
                       const QString &originalPath,
                       const QString &instruction)>
        requestEditNoteReview;
  };

  virtual ~OverseerTool() = default;

  virtual QString name() const = 0;

  virtual QString description() const = 0;

  virtual QJsonObject parametersSchema() const = 0;

  virtual Result execute(const QJsonObject &arguments,
                         const Context &context) const = 0;

  QJsonObject toSchema() const;
};