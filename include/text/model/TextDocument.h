#ifndef EPISTEME_TEXTDOCUMENT_H
#define EPISTEME_TEXTDOCUMENT_H
#include <QTextDocument>

class TextDocument : public QTextDocument {
    Q_OBJECT

public:
    enum class Type {
        PlainText,
        Markdown
    };

    explicit TextDocument(QObject *parent = nullptr);

    QString filePath() const;
    void setFilePath(const QString &path);

    Type type() const;
    void setType(Type type);

private:
    QString path;
    Type docType = Type::PlainText;
};

#endif //EPISTEME_TEXTDOCUMENT_H