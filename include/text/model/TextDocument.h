#ifndef EPISTEME_TEXTEDITOR_H
#define EPISTEME_TEXTEDITOR_H
#include <qtextdocument.h>


class TextDocument: public QTextDocument {
    Q_OBJECT

public:
     TextDocument(QObject *parent = nullptr);
};



#endif //EPISTEME_TEXTEDITOR_H
