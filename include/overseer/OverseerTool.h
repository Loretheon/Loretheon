#pragma once

#include <QJsonObject>
#include <QString>

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
  };

  virtual ~OverseerTool() = default;

  virtual QString name() const = 0;

  virtual QString description() const = 0;

  virtual QJsonObject parametersSchema() const = 0;

  virtual Result execute(const QJsonObject &arguments,
                         const Context &context) const = 0;

  QJsonObject toSchema() const;
};