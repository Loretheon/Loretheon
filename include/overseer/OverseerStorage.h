#pragma once

#include <QString>

// Filesystem layout helpers for the Overseer feature.
//
//   <AppDataLocation>/Overseer/
//     memory.md                        (global, shared across sessions)
//     Sessions/
//       <name>/
//         transcript.md
//         overview.md
//         memory.md                    (session-scoped)
//         settings.json
//         output/

namespace OverseerStorage {

// The Overseer root directory. Created on demand by ensureRoot().
QString rootPath();

// Path of the global memory file.
QString memoryPath();

// Read and write the global memory file. Empty string on failure.
QString readMemory();
bool writeMemory(const QString &text);

// Append a fact under "## Accepted proposals" in the given memory file.
// Creates the file and the section if needed. Returns true on success.
bool appendFactToMemoryFile(const QString &path, const QString &fact);

// Remove the first line under "## Accepted proposals" whose trimmed
// content equals the given fact exactly. Returns true if a line was
// removed. The prose block above the section is left untouched.
bool removeFactFromMemoryFile(const QString &path, const QString &fact);

// Replace the first line under "## Accepted proposals" whose trimmed
// content equals `oldFact` with a line for `newFact`. If `oldFact` is
// empty, this behaves as append. If `newFact` is empty, this behaves
// as remove. Returns true if the file was rewritten.
bool replaceFactInMemoryFile(const QString &path, const QString &oldFact,
                             const QString &newFact);

// Ensure the Overseer root and Sessions directory exist. Returns true on
// success, false otherwise.
bool ensureRoot();

} // namespace OverseerStorage