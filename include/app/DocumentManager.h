#pragma once

#include <QObject>
#include <QList>
#include <QHash>
#include <QString>

#include "TextDocument.h"

class DocumentManager : public QObject
{
    Q_OBJECT

public:
    explicit DocumentManager(QObject *parent = nullptr);

    TextDocument* currentDocument() const;

public slots:
    void newFile();
    bool openFile(const QString& path);
    bool save();
    void closeCurrent();

    signals:
        void documentChanged(TextDocument* document);

private:
    QList<TextDocument*> documents;
    QHash<TextDocument*, QString> filePaths;

    TextDocument* current = nullptr;
};