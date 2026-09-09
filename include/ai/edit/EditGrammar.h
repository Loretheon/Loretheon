#pragma once

#include <QString>

namespace EditGrammar
{
inline QString gbnf()
{
  return QStringLiteral(
      "root      ::= ws object (ws separator ws object)* ws\n"
      "separator ::= \",\" | newline\n"
      "object    ::= \"{\" ws "
      "\"\\\"old_string\\\"\" ws \":\" ws string \",\" ws "
      "\"\\\"new_string\\\"\" ws \":\" ws string "
      "ws \"}\"\n"
      "string    ::= \"\\\"\" char* \"\\\"\"\n"
      "char      ::= [^\"\\\\] | \"\\\\\" "
      "([\"\\\\/bfnrt] | \"u\" [0-9a-fA-F]{4})\n"
      "ws        ::= [ \\t]*\n"
      "newline   ::= \"\\n\" | \"\\r\\n\"\n"
  );
}
}