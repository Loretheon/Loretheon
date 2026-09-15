#pragma once

#include <QString>

// Filesystem layout helpers for the Overseer feature.
//
//   <AppDataLocation>/Overseer/
//     memory.md
//     Sessions/
//       <name>/
//         transcript.md
//         overview.md
//         output/

namespace OverseerStorage {

// The Overseer root directory. Created on demand by ensureRoot().
QString rootPath();

// Path of the global memory file.
QString memoryPath();

// Read and write the global memory file. Empty string on failure.
QString readMemory();
bool writeMemory(const QString &text);

// Ensure the Overseer root and Sessions directory exist. Returns true on
// success, false otherwise.
bool ensureRoot();

} // namespace OverseerStorage