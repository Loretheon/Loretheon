#include "DocumentManager.h"

#include "Settings.h"

#include <QDir>
#include <QFile>
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

TextDocument::Type DocumentManager::typeForExtension(const QString &extension)
{
    if (extension.compare("md", Qt::CaseInsensitive) == 0)
        return TextDocument::Type::Markdown;

    return TextDocument::Type::PlainText;
}

QString DocumentManager::uniqueDefaultPath(const QString &baseName, const QString &extension) const
{
    const QDir root(Settings::getRootDirectory());

    QString candidate = root.filePath(baseName + "." + extension);

    int suffix = 2;

    while (QFile::exists(candidate))
    {
        candidate = root.filePath(QString("%1 %2.%3").arg(baseName).arg(suffix).arg(extension));
        ++suffix;
    }

    return candidate;
}

void DocumentManager::createDocument(TextDocument::Type type, const QString &extension)
{
    const QString path = uniqueDefaultPath("Untitled", extension);

    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[NEW] Failed to create:"
                   << path
                   << "Error:" << file.errorString();
        return;
    }

    if (!file.commit())
    {
        qWarning() << "[NEW] Commit failed:"
                   << path
                   << "Error:" << file.errorString();
        return;
    }

    auto *document = new TextDocument(this);
    document->setType(type);
    document->setFilePath(path);
    document->setModified(false);

    documents.append(document);
    current = document;

    emit documentChanged(current);
    emit documentCreated(path);
}

void DocumentManager::newTextFile()
{
    createDocument(TextDocument::Type::PlainText, "txt");
}

void DocumentManager::newMarkdownFile()
{
    createDocument(TextDocument::Type::Markdown, "md");
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

    qDebug() << "[OPEN] raw text length:" << text.length() << "content:" << text.left(200);

    const TextDocument::Type type = typeForExtension(QFileInfo(path).suffix());

    auto *document = new TextDocument(this);
    document->setPlainText(text);

    document->setType(type);
    document->setFilePath(path);
    document->setModified(false);

    documents.append(document);

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

    const QString path = current->filePath();

    if (path.isEmpty())
    {
        qWarning() << "[SAVE] Document has no path; cannot save";
        return false;
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

    current->setModified(false);

    qDebug() << "[SAVE] Saved successfully:" << path;

    return true;
}

bool DocumentManager::renameFile(const QString &oldPath, const QString &newPath)
{
    if (oldPath == newPath)
        return true;

    if (QFile::exists(newPath))
    {
        qWarning() << "[RENAME] Target already exists:" << newPath;
        return false;
    }

    if (!QFile::rename(oldPath, newPath))
    {
        qWarning() << "[RENAME] Failed:"
                   << oldPath
                   << "->"
                   << newPath;
        return false;
    }

    for (TextDocument *document : std::as_const(documents))
    {
        if (document->filePath() != oldPath)
            continue;

        document->setFilePath(newPath);
        document->setType(typeForExtension(QFileInfo(newPath).suffix()));
        break;
    }

    emit fileRenamed(oldPath, newPath);
    return true;
}

void DocumentManager::closeCurrent()
{
    if (!current)
        return;

    TextDocument *toClose = current;

    documents.removeOne(toClose);

    current = documents.isEmpty()
        ? nullptr
        : documents.last();

    delete toClose;

    emit documentChanged(current);
}