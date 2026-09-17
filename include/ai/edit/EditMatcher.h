#pragma once

#include "EditCommand.h"
#include "EditMatch.h"

#include "../../text/model/TextDocument.h"

#include <edlib.h>

#include <QVector>

class EditMatcher {
public:
  struct Result {
    QVector<EditMatch> candidates;
    bool fuzzy = false;

    bool isEmpty() const { return candidates.isEmpty(); }
  };

  Result find(const TextDocument &document, const EditCommand &command) const;

private:
  Result findExact(const QString &text, const QString &needle,
                   int offset) const;

  Result findFuzzy(const QString &text, const QString &needle,
                   int offset) const;

  Result findWholeScopeBody(const TextDocument &document,
                            const EditCommand &command) const;
};