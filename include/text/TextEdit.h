#ifndef EPISTEME_TEXTEDIT_H
#define EPISTEME_TEXTEDIT_H

#include <QTextEdit>


class TextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit TextEdit(QWidget *parent = nullptr);

public slots:
    void bold();

    void italic();

    void leftAlign();

    void rightAlign();

    void justify();

    void center();

    void setLineSpacing();

    void setParagraphSpacing();
};

#endif // EPISTEME_TEXTEDIT_H