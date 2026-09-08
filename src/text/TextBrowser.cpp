#include "../../include/text/TextBrowser.h"

TextBrowser::TextBrowser(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenExternalLinks(true);
}