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

    std::function<void(const QString &copyPath,
                       const QString &originalPath,
                       const QString &instruction)>
        requestEditNoteReview;
  };

  virtual ~OverseerTool() = default;

  virtual QString name() const = 0;

  virtual QString description() const = 0;

  // Category drives filtering in the transcript UI.
  // Standard categories: "read", "write", "edit", "proposal", "stage".
  virtual QString category() const { return QStringLiteral("other"); }

  virtual QJsonObject parametersSchema() const = 0;

  virtual Result execute(const QJsonObject &arguments,
                         const Context &context) const = 0;

  QJsonObject toSchema() const;
};