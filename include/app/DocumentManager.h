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
  QList<TextDocument *> openDocuments() const { return openDocumentsList; }

  void setCurrentDocument(TextDocument *document);

public slots:
  void newTextFile();
  void newMarkdownFile();
  void newPlantUmlFile();
  void newTextFileIn(const QString &parentPath);
  void newMarkdownFileIn(const QString &parentPath);
  void newPlantUmlFileIn(const QString &parentPath);
  void newFolderIn(const QString &parentPath);
  bool openFile(const QString &path);
  bool save();
  bool saveDocument(TextDocument *document);
  bool renameFile(const QString &oldPath, const QString &newPath);
  bool deleteFile(const QString &path);
  bool convertToMarkdown(const QString &path);
  bool convertToText(const QString &path);
  bool convertToDot(const QString &path);
  bool convertToPlantUml(const QString &path);
  void closeCurrent();
  void closeDocument(TextDocument *document);

signals:
  void currentDocumentChanged(TextDocument *document);
  void documentOpened(TextDocument *document);
  void documentClosed(TextDocument *document);
  void documentChanged(TextDocument *document);

  // Emitted after a document's contents are written to disk
  // successfully. Carries the document so the receiver can read its
  // path and its body.
  void documentSaved(TextDocument *document);

  // Emitted when openFile() is asked for a media file. The manager does
  // not open a tab; whoever listens (DocumentArea, or MainWindow) is
  // responsible for showing the file.
  void mediaFileRequested(const QString &absolutePath);
  void unsupportedFileRequested(const QString &absolutePath,
                                const QString &reason);
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

  TextDocument *createDocument(DocumentMode type, const QString &extension);
  TextDocument *createDocumentIn(DocumentMode type, const QString &extension,
                                 const QString &parentPath);

  TextDocument *openDocumentFromPath(const QString &path);

  bool convertFile(const QString &path, const QString &targetExtension,
                   DocumentMode targetType);

  void registerOpenDocument(TextDocument *document);
  void unregisterOpenDocument(TextDocument *document);

  QList<TextDocument *> allDocuments;
  QList<TextDocument *> openDocumentsList;

  TextDocument *current = nullptr;
};

#endif // EPISTEME_DOCUMENTMANAGER_H