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

  // The document belonging to the currently focused tab. Null if no tabs.
  TextDocument *currentDocument() const;

  // All documents that currently have an open tab (in tab order).
  QList<TextDocument *> openDocuments() const { return openDocumentsList; }

  // Called by DocumentArea when the user switches tabs. Fires
  // currentDocumentChanged if the pointer actually changed.
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
  // Fired when a document becomes the focused one.
  void currentDocumentChanged(TextDocument *document);

  // Fired when a new document is created and a tab should be added.
  void documentOpened(TextDocument *document);

  // Fired when a document is closed and its tab should be removed.
  void documentClosed(TextDocument *document);

  // Kept for compatibility with existing wiring; fires on any change to
  // the focused document's metadata (path, type, modified flag).
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

  // Creates a document, opens a tab for it, and makes it current.
  TextDocument *createDocument(DocumentMode type, const QString &extension);
  TextDocument *createDocumentIn(DocumentMode type, const QString &extension,
                                 const QString &parentPath);

  TextDocument *openDocumentFromPath(const QString &path);

  bool convertFile(const QString &path, const QString &targetExtension,
                   DocumentMode targetType);

  void registerOpenDocument(TextDocument *document);
  void unregisterOpenDocument(TextDocument *document);

  // Every TextDocument ever created and still alive. Owned by this manager.
  QList<TextDocument *> allDocuments;

  // Subset of allDocuments that have an open tab, in tab order.
  QList<TextDocument *> openDocumentsList;

  TextDocument *current = nullptr;
};

#endif // EPISTEME_DOCUMENTMANAGER_H