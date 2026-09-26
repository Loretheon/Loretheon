#pragma once

#include <QJsonObject>
#include <QString>

#include <functional>

class TextDocument;
class TextEdit;

class Tool {
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

    QString focusedFilePath;
    TextDocument *focusedDocument = nullptr;
    TextEdit *focusedEditor = nullptr;

    std::function<void(const QString &copyPath,
                       const QString &originalPath,
                       const QString &instruction)>
        requestEditNoteReview;

    std::function<void(const QString &absolutePath)> openFile;

    std::function<void(const QString &absolutePath)> closeFile;

    std::function<void(TextEdit *editor, TextDocument *document,
                       const QString &instruction)>
        requestScopedEdit;
  };

  virtual ~Tool() = default;

  virtual QString name() const = 0;

  virtual QString description() const = 0;

  virtual QString category() const { return QStringLiteral("other"); }

  virtual QJsonObject parametersSchema() const = 0;

  virtual Result execute(const QJsonObject &arguments,
                         const Context &context) const = 0;

  QJsonObject toSchema() const;
};