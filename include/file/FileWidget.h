#ifndef EPISTEME_FILEWIDGET_H
#define EPISTEME_FILEWIDGET_H

#include <QWidget>

#include "FileSystemModel.h"
#include "FileSystemView.h"

class FileWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileWidget(QWidget *parent = nullptr);

public slots:
    void beginEditingPath(const QString &path);

    signals:
        void fileSelected(const QString &path);
    void renameRequested(const QString &oldPath, const QString &newPath);

private:
    FileSystemModel *fileSystemModel;
    FileSystemView *fileSystemView;

    QString pendingEditPath;
};

#endif //EPISTEME_FILEWIDGET_H