#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "TextDocument.h"

class DocumentManager : public QObject {
  Q_OBJECT

public:
  explicit DocumentManager(QObject *parent = nullptr);

  TextDocument *currentDocument() const;

public slots:
  void newTextFile();
  void newMarkdownFile();
  bool openFile(const QString &path);
  bool save();
  bool renameFile(const QString &oldPath, const QString &newPath);
  void closeCurrent();

signals:
  void documentChanged(TextDocument *document);
  void documentCreated(const QString &path);
  void fileRenamed(const QString &oldPath, const QString &newPath);

private:
  static DocumentMode typeForExtension(const QString &extension);

  QString uniqueDefaultPath(const QString &baseName,
                            const QString &extension) const;
  void createDocument(DocumentMode type, const QString &extension);

  QList<TextDocument *> documents;

  TextDocument *current = nullptr;
};