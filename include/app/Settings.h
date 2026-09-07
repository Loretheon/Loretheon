#ifndef EPISTEME_SETTINGS_H
#define EPISTEME_SETTINGS_H
#include <QString>


class Settings {
public:
    static QString getRootDirectory();
    static void setRootDirectory(const QString& newRoot);
};


#endif //EPISTEME_SETTINGS_H
