#ifndef EPISTEME_DOCUMENTMANAGER_H
#define EPISTEME_DOCUMENTMANAGER_H

#include <QList>
#include <QObject>
#include <QString>

#include "TextDocument.h"

class QDir;
class DocumentManager : public QObject {
  Q_OBJECT

public:
  explicit DocumentManager(QObject *parent = nullptr);

  TextDocument *currentDocument() const;

public slots:
  void newTextFile();
  void newMarkdownFile();
  void newTextFileIn(const QString &parentPath);
  void newMarkdownFileIn(const QString &parentPath);
  void newFolderIn(const QString &parentPath);
  bool openFile(const QString &path);
  bool save();
  bool renameFile(const QString &oldPath, const QString &newPath);
  bool deleteFile(const QString &path);
  bool convertToMarkdown(const QString &path);
  bool convertToText(const QString &path);
  void closeCurrent();

  signals:
    void documentChanged(TextDocument *document);
  void documentCreated(const QString &path);
  void fileRenamed(const QString &oldPath, const QString &newPath);
  void fileDeleted(const QString &path);
  void folderCreated(const QString &path);
  void fileConverted(const QString &oldPath, const QString &newPath);

private:
  static DocumentMode typeForExtension(const QString &extension);

  QString uniqueDefaultPath(const QString &baseName,
                            const QString &extension) const;
  QString uniquePathIn(const QDir &dir, const QString &baseName,
                       const QString &extension) const;
  QString uniqueFolderPathIn(const QDir &dir, const QString &baseName) const;
  void createDocument(DocumentMode type, const QString &extension);
  void createDocumentIn(DocumentMode type, const QString &extension,
                        const QString &parentPath);
  bool convertFile(const QString &path, const QString &targetExtension,
                   DocumentMode targetType);

  QList<TextDocument *> documents;

  TextDocument *current = nullptr;
};

#endif // EPISTEME_DOCUMENTMANAGER_H