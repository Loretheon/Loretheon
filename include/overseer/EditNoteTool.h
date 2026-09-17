#pragma once

#include "OverseerTool.h"

class EditNoteTool : public OverseerTool {
public:
  QString name() const override { return QStringLiteral("edit_note"); }

  QString description() const override {
    return QStringLiteral(
        "Open a referenced note for editing in a session-local copy. The "
        "user will review and apply changes; the original note is not "
        "modified by this tool. Use this to make structured edits to a file "
        "the user has referenced in the Overview. The path must be under "
        "the user's notes root.");
  }

  QJsonObject parametersSchema() const override;

  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};