#ifndef EPISTEME_TEXTDOCUMENT_H
#define EPISTEME_TEXTDOCUMENT_H
#include "DocumentMode.h"

#include <QTextDocument>

class TextDocument : public QTextDocument {
  Q_OBJECT

public:
  explicit TextDocument(QObject *parent = nullptr);

  QString filePath() const;
  void setFilePath(const QString &path);

  DocumentMode type() const;
  void setType(DocumentMode type);

private:
  QString path;
  DocumentMode docType = DocumentMode::PlainText;
};

#endif // EPISTEME_TEXTDOCUMENT_H