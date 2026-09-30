#pragma once

#include <QString>
#include <QStringList>

// Reads and writes the small, curated, human-editable files that make
// up the assistant's always-in-context knowledge:
//
//   identity.md   — who she is. Fixed by the app, editable by the user
//                   and by the assistant.
//   user.md       — what she knows about the user.
//   self.md       — what she knows about herself and her own history.
//
// These files are read at startup and on every change. They are small
// on purpose. Anything that grows unbounded belongs in memories/, not
// here.
//
// The files live under a caller-provided root directory. The app sets
// it once at startup. There is no default and no fallback.
class AssistantProfile {
public:
  AssistantProfile() = default;

  // Root directory. Must be set before load() or save().
  void setRoot(const QString &root);
  QString root() const { return m_root; }

  // Load all three files. Missing files are created with defaults.
  // Returns false only if the directory cannot be created.
  bool load();

  // Re-read all three files from disk, discarding any in-memory
  // changes. Used after an external editor or a profile edit job has
  // written the files.
  bool reload();

  // Persist all three files. Creates the directory if needed.
  bool save();

  // The three bodies as loaded. Empty until load() succeeds.
  QString identity() const { return m_identity; }
  QString user() const { return m_user; }
  QString self() const { return m_self; }

  // Replace and mark dirty. Call save() to persist.
  void setIdentity(const QString &text);
  void setUser(const QString &text);
  void setSelf(const QString &text);

  // True if any body has been changed since the last load or save.
  bool isDirty() const { return m_dirty; }

  // Absolute paths for the three files. Useful for the settings panel
  // and for shell commands the user can copy.
  QString identityPath() const;
  QString userPath() const;
  QString selfPath() const;

  // The absolute path of a file under the assistant root, whether or
  // not it exists. Does not create anything.
  QString pathFor(const QString &relative) const;

  // Ensure the root directory exists. Returns false on failure.
  bool ensureRoot() const;

  // The default bodies written the first time the files are created.
  static QString defaultIdentity();
  static QString defaultUser();
  static QString defaultSelf();

private:
  QString m_root;

  QString m_identity;
  QString m_user;
  QString m_self;

  bool m_dirty = false;

  static bool readFile(const QString &path, QString &out);
  static bool writeFile(const QString &path, const QString &content);
};