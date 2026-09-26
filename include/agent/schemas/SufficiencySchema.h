#pragma once

#include "../StructuredSchema.h"

#include <QString>

// The schema used to judge whether a set of notes contains enough
// information to answer a question. The model returns:
//
//   {"sufficient": true|false, "reason": "..."}
class SufficiencySchema {
public:
  static StructuredSchema schema();

  // The system prompt that frames the model's role for this call.
  static QString systemPrompt();

  // Build the user prompt from a question and the concatenated note
  // bodies.
  static QString userPrompt(const QString &question,
                            const QString &notes);
};