// EditGrammar.h
#pragma once

#include <QString>
#include <QStringList>

namespace EditGrammar {

inline QString escapeLiteral(const QString &text) {
  QString escaped = text;

  escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));

  escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));

  return escaped;
}

inline QString makeAlternatives(const QStringList &values) {
  QStringList alternatives;

  alternatives.reserve(values.size());

  for (const QString &value : values) {
    alternatives.append(QStringLiteral("\"%1\"").arg(escapeLiteral(value)));
  }

  if (alternatives.isEmpty()) {
    return QStringLiteral("\"\"");
  }

  return alternatives.join(QStringLiteral(" | "));
}

/*
 * Bounded array of edit commands.
 *
 * GBNF has no native {0,N} repetition syntax, so the
 * "continue or stop" tail is manually unrolled into a
 * finite chain of numbered rules. Each tail rule offers
 * the model a choice between stopping (empty) or emitting
 * one more comma-separated command and advancing to the
 * next tail rule. The final rule in the chain only accepts
 * empty, which forces termination.
 *
 * Without this cap, "" | ws "," ws command commandTail is
 * simultaneously valid at every position after a complete
 * command, so a model that leans toward "continue" under
 * grammar-constrained sampling can generate an unbounded
 * stream of syntactically valid edits with no way to stop.
 * This happened in practice and hung the inference server
 * generating tokens indefinitely.
 *
 * kMaxEditsPerPlan controls the cap. Raise it if legitimate
 * requests need more edits than the cap currently allows.
 */
inline constexpr int kMaxEditsPerPlan = 8;

inline QString makeBoundedTailChain() {
  QString rules;

  for (int i = 0; i < kMaxEditsPerPlan - 1; ++i) {

    const int nextIndex = i + 1;

    if (nextIndex < kMaxEditsPerPlan - 1) {

      rules += QStringLiteral("commandTail%1 ::= "
                              "\"\" | ws \",\" ws command commandTail%2\n")
                   .arg(i)
                   .arg(nextIndex);

    } else {

      /*
       * Final tail rule in the chain: no further
       * continuation is offered, so the grammar has
       * no valid path except closing the array here.
       */
      rules += QStringLiteral("commandTail%1 ::= \"\"\n").arg(i);
    }
  }

  return rules;
}

inline QString gbnf(const QStringList &scopeIds) {
  QString grammar =
      QStringLiteral("root ::= ws \"[\" ws editList ws \"]\" ws\n"

                     "editList ::= \"\" | command commandTail0\n");

  grammar += makeBoundedTailChain();

  grammar += QStringLiteral("command ::= insert | replace | delete\n"

                            "insert ::= \"{\" ws "
                            "\"\\\"operation\\\"\" ws \":\" ws "
                            "\"\\\"insert\\\"\" ws \",\" ws "
                            "\"\\\"scope\\\"\" ws \":\" ws "
                            "\"\\\"\" scopeValue \"\\\"\" ws \",\" ws "
                            "\"\\\"position\\\"\" ws \":\" ws "
                            "\"\\\"\" positionValue \"\\\"\" ws \",\" ws "
                            "\"\\\"find\\\"\" ws \":\" ws "
                            "\"\\\"\\\"\" ws \",\" ws "
                            "\"\\\"all\\\"\" ws \":\" ws "
                            "boolean ws \",\" ws "
                            "\"\\\"instruction\\\"\" ws \":\" ws "
                            "string ws \"}\"\n"

                            "replace ::= \"{\" ws "
                            "\"\\\"operation\\\"\" ws \":\" ws "
                            "\"\\\"replace\\\"\" ws \",\" ws "
                            "\"\\\"scope\\\"\" ws \":\" ws "
                            "\"\\\"\" scopeValue \"\\\"\" ws \",\" ws "
                            "\"\\\"position\\\"\" ws \":\" ws "
                            "\"\\\"\" positionValue \"\\\"\" ws \",\" ws "
                            "\"\\\"find\\\"\" ws \":\" ws "
                            "string ws \",\" ws "
                            "\"\\\"all\\\"\" ws \":\" ws "
                            "boolean ws \",\" ws "
                            "\"\\\"instruction\\\"\" ws \":\" ws "
                            "string ws \"}\"\n"

                            "delete ::= \"{\" ws "
                            "\"\\\"operation\\\"\" ws \":\" ws "
                            "\"\\\"delete\\\"\" ws \",\" ws "
                            "\"\\\"scope\\\"\" ws \":\" ws "
                            "\"\\\"\" scopeValue \"\\\"\" ws \",\" ws "
                            "\"\\\"position\\\"\" ws \":\" ws "
                            "\"\\\"\" positionValue \"\\\"\" ws \",\" ws "
                            "\"\\\"find\\\"\" ws \":\" ws "
                            "string ws \",\" ws "
                            "\"\\\"all\\\"\" ws \":\" ws "
                            "boolean ws \",\" ws "
                            "\"\\\"instruction\\\"\" ws \":\" ws "
                            "string ws \"}\"\n"

                            "scopeValue ::= %1\n"

                            "positionValue ::= "
                            "\"before\" | "
                            "\"after\"\n"

                            "boolean ::= "
                            "\"true\" | "
                            "\"false\"\n"

                            "string ::= "
                            "\"\\\"\" char* \"\\\"\"\n"

                            "char ::= "
                            "[^\"\\\\\\x7F\\x00-\\x1F] | "
                            "\"\\\\\" "
                            "([\"\\\\/bfnrt] | "
                            "\"u\" [0-9a-fA-F]{4})\n"

                            "ws ::= [ \\t]*\n")
                 .arg(makeAlternatives(scopeIds));

  return grammar;
}

} // namespace EditGrammar