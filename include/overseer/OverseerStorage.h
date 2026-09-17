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

// Ensure the Overseer root and Sessions directory exist. Returns true on
// success, false otherwise.
bool ensureRoot();

} // namespace OverseerStorage