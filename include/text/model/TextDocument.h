#ifndef EPISTEME_TEXTDOCUMENT_H
#define EPISTEME_TEXTDOCUMENT_H
#include <QTextDocument>

class TextDocument : public QTextDocument {
    Q_OBJECT

public:
    explicit TextDocument(QObject *parent = nullptr);
};

#endif //EPISTEME_TEXTDOCUMENT_H