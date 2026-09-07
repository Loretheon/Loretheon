#include "DocumentManager.h"

#include <QFile>
#include <QTextStream>


DocumentManager::DocumentManager(QObject *parent)
    : QObject(parent)
{
}


TextDocument* DocumentManager::currentDocument() const
{
    return current;
}


void DocumentManager::newFile()
{
    auto *document = new TextDocument(this);

    documents.append(document);
    current = document;

    emit documentChanged(current);
}


void DocumentManager::openFile(const QString& path)
{
    auto *document = new TextDocument(this);

    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete document;
        return;
    }

    QTextStream stream(&file);
    document->setPlainText(stream.readAll());

    documents.append(document);
    filePaths.insert(document, path);

    current = document;

    emit documentChanged(current);
}


void DocumentManager::save()
{
    if (!current)
    {
        qDebug() << "[SAVE] No current document";
        return;
    }

    const QString path = filePaths.value(current);

    qDebug() << "[SAVE] Path:" << path;
    qDebug() << "[SAVE] Document contents:";
    qDebug().noquote() << current->toPlainText();

    if (path.isEmpty())
    {
        qDebug() << "[SAVE] No file path associated with document";
        return;
    }

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "[SAVE] Failed to open file:" << file.errorString();
        return;
    }

    QTextStream stream(&file);
    stream << current->toPlainText();

    file.close();

    current->setModified(false);

    qDebug() << "[SAVE] Saved successfully:" << !current->isModified();
}

void DocumentManager::closeCurrent()
{
    if (!current)
        return;

    filePaths.remove(current);
    documents.removeOne(current);

    delete current;

    current = documents.isEmpty()
        ? nullptr
        : documents.last();

    emit documentChanged(current);
}