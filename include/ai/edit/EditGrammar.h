#pragma once

#include <QString>

namespace EditGrammar {

inline QString gbnf() {
  return QStringLiteral(
      "root ::= ws object (ws newline ws object)* ws\n"
      "object ::= \"{\" ws "
      "\"\\\"operation\\\"\" ws \":\" ws string \",\" ws "
      "\"\\\"scope\\\"\" ws \":\" ws string \",\" ws "
      "\"\\\"position\\\"\" ws \":\" ws string \",\" ws "
      "\"\\\"find\\\"\" ws \":\" ws string \",\" ws "
      "\"\\\"new\\\"\" ws \":\" ws string \",\" ws "
      "\"\\\"all\\\"\" ws \":\" ws boolean "
      "ws \"}\"\n"
      "string ::= \"\\\"\" char* \"\\\"\"\n"
      "char ::= [^\"\\\\\\x7F\\x00-\\x1F] | \"\\\\\" "
      "([\"\\\\/bfnrt] | \"u\" [0-9a-fA-F]{4})\n"
      "boolean ::= \"true\" | \"false\"\n"
      "ws ::= [ \\t]*\n"
      "newline ::= \"\\n\" | \"\\r\\n\"\n");
}

} // namespace EditGrammar