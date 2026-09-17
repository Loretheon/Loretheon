#pragma once

#include <QString>

// Per-session toggles for Overseer automation. Stored as a small JSON
// file inside the session folder.
//
// Semantics:
//   - automatic  : the general "leave me alone" mode. When on, it
//                  forces autoMemory and autoEdits on, and the UI hides
//                  the manual review affordances.
//   - autoMemory : accept every memory proposal without asking.
//   - autoEdits  : apply every edit plan without asking.

struct SessionSettings {
  bool automatic = false;
  bool autoMemory = false;
  bool autoEdits = false;

  // Load from <session folder>/settings.json. Missing file or malformed
  // JSON yields defaults. Applies the "automatic implies both" rule on
  // load.
  static SessionSettings load(const QString &path);

  // Save to <session folder>/settings.json. Creates parent directories
  // if they are missing. Returns false on any failure. Applies the
  // "automatic implies both" rule before writing.
  bool save(const QString &path) const;

  // Ensure a file exists at `path`, writing defaults if not. Returns
  // true if a file exists after the call (either it was already there or
  // it was created).
  static bool ensureFile(const QString &path);

  // Resolve the effective values after applying the implication rule.
  // Mutates in place.
  void normalize();

  bool effectiveAutoMemory() const { return automatic || autoMemory; }
  bool effectiveAutoEdits() const { return automatic || autoEdits; }
};