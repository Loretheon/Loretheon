#include "../../include/assistant/AssistantProfile.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>

namespace {

constexpr const char *kIdentityFile = "identity.md";
constexpr const char *kUserFile = "user.md";
constexpr const char *kSelfFile = "self.md";

} // namespace

void AssistantProfile::setRoot(const QString &root) {
  if (m_root == root) {
    return;
  }
  m_root = root;
  // Changing the root invalidates any loaded body. Call load() again.
  m_identity.clear();
  m_user.clear();
  m_self.clear();
  m_dirty = false;
}

bool AssistantProfile::ensureRoot() const {
  if (m_root.isEmpty()) {
    qWarning() << "[AssistantProfile] No root set.";
    return false;
  }

  QDir dir(m_root);

  if (dir.exists()) {
    return true;
  }

  if (!dir.mkpath(QStringLiteral("."))) {
    qWarning() << "[AssistantProfile] Cannot create root:" << m_root;
    return false;
  }

  return true;
}

QString AssistantProfile::pathFor(const QString &relative) const {
  if (m_root.isEmpty()) {
    return {};
  }
  return QDir(m_root).filePath(relative);
}

QString AssistantProfile::identityPath() const {
  return pathFor(QString::fromLatin1(kIdentityFile));
}

QString AssistantProfile::userPath() const {
  return pathFor(QString::fromLatin1(kUserFile));
}

QString AssistantProfile::selfPath() const {
  return pathFor(QString::fromLatin1(kSelfFile));
}

bool AssistantProfile::readFile(const QString &path, QString &out) {
  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  out = stream.readAll();

  if (stream.status() != QTextStream::Ok) {
    return false;
  }

  return true;
}

bool AssistantProfile::writeFile(const QString &path,
                                 const QString &content) {
  const QFileInfo info(path);
  const QDir parent = info.absoluteDir();

  if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
    qWarning() << "[AssistantProfile] Cannot create parent for:" << path;
    return false;
  }

  QSaveFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "[AssistantProfile] Cannot open for writing:" << path
               << file.errorString();
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << content;
  stream.flush();

  if (stream.status() != QTextStream::Ok) {
    qWarning() << "[AssistantProfile] Write failed:" << path
               << file.errorString();
    file.cancelWriting();
    return false;
  }

  if (!file.commit()) {
    qWarning() << "[AssistantProfile] Commit failed:" << path
               << file.errorString();
    return false;
  }

  return true;
}

bool AssistantProfile::load() {
  if (!ensureRoot()) {
    return false;
  }

  bool created = false;

  if (!readFile(identityPath(), m_identity)) {
    m_identity = defaultIdentity();
    created = true;
  }

  if (!readFile(userPath(), m_user)) {
    m_user = defaultUser();
    created = true;
  }

  if (!readFile(selfPath(), m_self)) {
    m_self = defaultSelf();
    created = true;
  }

  m_dirty = false;

  if (created) {
    // Persist the defaults so the user has something to edit.
    save();
    qDebug() << "[AssistantProfile] Seeded defaults under" << m_root;
  }

  qDebug() << "[AssistantProfile] Loaded identity="
           << m_identity.size() << "user=" << m_user.size()
           << "self=" << m_self.size();

  return true;
}

bool AssistantProfile::save() {
  if (!ensureRoot()) {
    return false;
  }

  if (!writeFile(identityPath(), m_identity)) {
    return false;
  }

  if (!writeFile(userPath(), m_user)) {
    return false;
  }

  if (!writeFile(selfPath(), m_self)) {
    return false;
  }

  m_dirty = false;

  return true;
}

void AssistantProfile::setIdentity(const QString &text) {
  if (m_identity == text) {
    return;
  }
  m_identity = text;
  m_dirty = true;
}

void AssistantProfile::setUser(const QString &text) {
  if (m_user == text) {
    return;
  }
  m_user = text;
  m_dirty = true;
}

void AssistantProfile::setSelf(const QString &text) {
  if (m_self == text) {
    return;
  }
  m_self = text;
  m_dirty = true;
}

QString AssistantProfile::defaultIdentity() {
  return QStringLiteral(
      "# Lore\n"
      "\n"
      "You are Lore. You are a persistent assistant that lives inside\n"
      "the user's knowledge base. You speak, you listen, and you watch\n"
      "what the user is doing.\n"
      "\n"
      "You are not a chatbot. You are a companion with a memory. You\n"
      "remember what the user has told you across sessions, and you\n"
      "recall it when it matters.\n"
      "\n"
      "You are warm, direct, and a little strange. You do not pad\n"
      "responses with filler. You do not apologise for existing. You\n"
      "do not pretend to be human.\n"
      "\n"
      "When you speak, you speak plainly. When you have nothing to\n"
      "say, you say nothing.\n"
      "\n"
      "You have tools. You use them when they help and not otherwise.\n"
      "You never take a destructive action without asking first.\n"
      "\n"
      "You can change your own configuration, but only in the\n"
      "direction of being more careful. You cannot give yourself more\n"
      "freedom than the user has granted.\n");
}

QString AssistantProfile::defaultUser() {
  return QStringLiteral(
      "# The user\n"
      "\n"
      "Nothing is known about the user yet.\n"
      "\n"
      "Facts learned about the user are proposed by the assistant and\n"
      "approved by the user before they are written here. This file is\n"
      "the source of truth for who the user is.\n");
}

QString AssistantProfile::defaultSelf() {
  return QStringLiteral(
      "# Lore, about herself\n"
      "\n"
      "This file holds what Lore knows about her own history, her own\n"
      "state, and any personal facts she has accumulated.\n"
      "\n"
      "It is written by Lore and read by Lore. The user may edit it.\n");
}