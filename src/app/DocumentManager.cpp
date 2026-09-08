#include "DocumentManager.h"

#include "Settings.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QDebug>

DocumentManager::DocumentManager(QObject *parent)
    : QObject(parent),
      current(nullptr)
{
}

TextDocument *DocumentManager::currentDocument() const
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

bool DocumentManager::openFile(const QString &path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "[OPEN] Failed to open:"
                   << path
                   << file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    const QString text = stream.readAll();

    if (stream.status() != QTextStream::Ok)
    {
        qWarning() << "[OPEN] Failed while reading:"
                   << path;
        return false;
    }

    auto *document = new TextDocument(this);
    document->setPlainText(text);
    document->setModified(false);

    documents.append(document);
    filePaths.insert(document, path);

    current = document;

    emit documentChanged(current);
    return true;
}

bool DocumentManager::save()
{
    if (!current)
    {
        qWarning() << "[SAVE] No current document";
        return false;
    }

    QString path = filePaths.value(current);

    if (path.isEmpty())
    {
        const QString textFilter = tr("Text Files (*.txt)");
        const QString allFilesFilter = tr("All Files (*)");

        QString selectedFilter;

        path = QFileDialog::getSaveFileName(
            nullptr,
            tr("Save File"),
            Settings::getRootDirectory(),
            textFilter + ";;" + allFilesFilter,
            &selectedFilter
        );

        if (path.isEmpty())
        {
            qDebug() << "[SAVE] Save cancelled";
            return false;
        }

        if (selectedFilter == textFilter)
        {
            const QFileInfo fileInfo(path);

            if (fileInfo.suffix().isEmpty())
                path += ".txt";
        }
    }

    qDebug() << "[SAVE] Path:" << path;

    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[SAVE] Failed to open:"
                   << path
                   << "Error:" << file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    stream << current->toPlainText();
    stream.flush();

    if (stream.status() != QTextStream::Ok)
    {
        qWarning() << "[SAVE] Failed while writing:"
                   << path
                   << "Error:" << file.errorString();

        file.cancelWriting();
        return false;
    }

    if (!file.commit())
    {
        qWarning() << "[SAVE] Commit failed:"
                   << path
                   << "Error:" << file.errorString();
        return false;
    }

    filePaths.insert(current, path);
    current->setModified(false);

    qDebug() << "[SAVE] Saved successfully:" << path;

    return true;
}

void DocumentManager::closeCurrent()
{
    if (!current)
        return;

    TextDocument *toClose = current;

    filePaths.remove(toClose);
    documents.removeOne(toClose);

    current = documents.isEmpty()
        ? nullptr
        : documents.last();

    delete toClose;

    emit documentChanged(current);
}