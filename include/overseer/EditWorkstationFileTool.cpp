#include "../../include/overseer/EditWorkstationFileTool.h"

#include <QJsonArray>
#include <QJsonObject>

QString EditWorkstationFileTool::description() const {
  return QStringLiteral(
      "Open a scoped edit session on the file the user is currently "
      "looking at in the Workstation. The user reviews and approves the "
      "generated edits in the transcript before they are applied. Use "
      "this when the user asks to rewrite, edit, or restructure the "
      "file they have open. Do not pass a path; the tool always targets "
      "the currently focused Workstation window.");
}

QJsonObject EditWorkstationFileTool::parametersSchema() const {
  QJsonObject instruction;
  instruction.insert(QStringLiteral("type"), QStringLiteral("string"));
  instruction.insert(
      QStringLiteral("description"),
      QStringLiteral("A plain-language description of the edit. "
                     "Examples: 'make the intro shorter', 'rewrite the "
                     "Summary section', 'replace X with Y in the "
                     "Conventions section'."));

  QJsonObject properties;
  properties.insert(QStringLiteral("instruction"), instruction);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("instruction")});

  return schema;
}

OverseerTool::Result
EditWorkstationFileTool::execute(const QJsonObject &arguments,
                                 const Context &context) const {
  Result result;

  const QString instruction =
      arguments.value(QStringLiteral("instruction")).toString().trimmed();

  if (instruction.isEmpty()) {
    result.ok = false;
    result.error = QStringLiteral("'instruction' is required.");
    return result;
  }

  if (!context.focusedDocument || !context.focusedEditor) {
    result.ok = false;
    result.error = QStringLiteral(
        "No Workstation window is currently focused. Click a window "
        "first, then ask to edit it.");
    return result;
  }

  if (!context.requestScopedEdit) {
    result.ok = false;
    result.error = QStringLiteral(
        "The host did not provide a scoped edit callback.");
    return result;
  }

  context.requestScopedEdit(context.focusedEditor, context.focusedDocument,
                            instruction);

  result.ok = true;
  result.output = QStringLiteral(
      "Scoped edit session started on the focused file. The user will "
      "review the generated edits in the transcript.");
  return result;
}