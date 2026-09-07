#ifndef EPISTEME_FILESYSTEMMODEL_H
#define EPISTEME_FILESYSTEMMODEL_H
#include <QFileSystemModel>


class FileSystemModel: public QFileSystemModel{
    Q_OBJECT

public:
    explicit FileSystemModel(QObject *parent = nullptr);
};


#endif //EPISTEME_FILESYSTEMMODEL_H
