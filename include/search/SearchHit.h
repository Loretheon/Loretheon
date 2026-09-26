#pragma once

#include <QString>

// One result from a semantic search. Carries the file and scope
// identifiers so the caller can open the source, plus the text so the
// caller can show a snippet without reading the file back.
struct SearchHit {
  QString filePath;       // absolute path to the note
  QString scopeId;        // DocumentNode id
  QString heading;        // heading line, or empty for the root scope
  QString body;           // the scope's text, trimmed
  float similarity = 0.0f; // cosine similarity, higher is better
  int64_t vectorId = -1;  // position in the FAISS index

  bool isValid() const { return !filePath.isEmpty() && vectorId >= 0; }
};