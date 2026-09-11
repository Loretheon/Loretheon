#pragma once

#include "EditCommand.h"
#include "EditMatch.h"

#include <QString>

struct PendingEdit {
  int id = 0;

  EditCommand command;

  EditMatch match;

  QString generatedText;

  bool accepted = true;

  bool completed = false;

  bool hasConflict = false;
};