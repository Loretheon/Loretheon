#pragma once

#include <QString>

// Per-session toggles for Overseer automation and a short, intentional
// description of what the session is about. Stored as a small JSON file
// inside the session folder.
//
// Semantics:
//   - automatic  : the general "leave me alone" mode. When on, it
//                  forces autoMemory and autoEdits on, and the UI hides
//                  the manual review affordances.
//   - autoMemory : accept every memory proposal without asking.
//   - autoEdits  : apply every edit plan without asking.
//
// The description is written once, when the session is created, by the
// human or by the model. It is read by the model when it is choosing
// which session a task belongs to. It is never rewritten automatically.
struct SessionSettings {
  bool automatic = false;
  bool autoMemory = false;
  bool autoEdits = false;

  QString description;

  static SessionSettings load(const QString &path);

  bool save(const QString &path) const;

  static bool ensureFile(const QString &path);

  void normalize();

  bool effectiveAutoMemory() const { return automatic || autoMemory; }
  bool effectiveAutoEdits() const { return automatic || autoEdits; }
};