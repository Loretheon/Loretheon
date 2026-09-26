#include "../../../include/agent/schemas/SufficiencySchema.h"

StructuredSchema SufficiencySchema::schema() {
  StructuredSchema schema(
      QStringLiteral("sufficiency"),
      QStringLiteral("Whether the provided notes answer the question."));

  schema.booleanField(
      QStringLiteral("sufficient"),
      QStringLiteral("True if the notes contain enough information to "
                     "answer the question. False otherwise."));

  schema.stringField(
      QStringLiteral("reason"),
      QStringLiteral("One short sentence explaining the decision."),
      /*required=*/false);

  return schema;
}

QString SufficiencySchema::systemPrompt() {
  return QStringLiteral(
      "You decide whether a set of notes contains enough information to "
      "answer a question. Be strict: if the notes only touch on the topic "
      "without actually answering, answer false. Return your decision "
      "using the provided JSON structure.");
}

QString SufficiencySchema::userPrompt(const QString &question,
                                      const QString &notes) {
  QString prompt;

  prompt += QStringLiteral("Question:\n");
  prompt += question;
  prompt += QStringLiteral("\n\nNotes:\n");
  prompt += notes;

  return prompt;
}