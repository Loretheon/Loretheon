#pragma once

#include "SearchHit.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

class InferenceService;
class VectorIndex;
class QTimer;

class ScopeIndex : public QObject {
  Q_OBJECT

public:
  struct Entry {
    QString filePath;
    QString scopeId;
    QString heading;
    QString body;
    QString contentHash;
  };

  explicit ScopeIndex(InferenceService *inference,
                      QObject *parent = nullptr);
  ~ScopeIndex() override;

  void setIndexDirectory(const QString &directory);
  QString indexDirectory() const { return m_indexDirectory; }

  void setAdditionalRoots(const QStringList &roots);
  QStringList additionalRoots() const { return m_additionalRoots; }

  bool load();

  int rebuild(const QString &notesRoot);

  int addFile(const QString &absolutePath);

  int removeFile(const QString &absolutePath);

  int refreshFile(const QString &absolutePath);

  QString previewFor(const QString &absolutePath) const;

  void markDirty(const QString &absolutePath);

  bool isReady() const;

  int64_t vectorCount() const;
  int64_t scopeCount() const;

  Entry entryFor(int64_t vectorId) const;

  VectorIndex *vectors() const { return m_vectors.get(); }

signals:
  void progress(int current, int total);
  void finished(int scopes);

private slots:
  void onDirtyTimer();

private:
  QStringList collectMarkdownFiles(const QString &root) const;
  QStringList collectAllMarkdownFiles(const QString &notesRoot) const;
  QVector<Entry> extractScopes(const QString &absolutePath) const;
  void collectNodes(const struct DocumentNode &node,
                    const QString &documentText,
                    const QString &filePath,
                    QVector<Entry> &out) const;

  bool saveSidecar(const QString &path) const;
  bool loadSidecar(const QString &path);



  InferenceService *m_inference = nullptr;

  QString m_indexDirectory;
  QStringList m_additionalRoots;

  std::unique_ptr<VectorIndex> m_vectors;
  QVector<Entry> m_entries;

  QSet<QString> m_dirtyPaths;
  QTimer *m_dirtyTimer = nullptr;

  int m_dimensions = 384;
  static constexpr int kSidecarVersion = 1;
};