#ifndef EPISTEME_FILEWIDGET_H
#define EPISTEME_FILEWIDGET_H

#include <QWidget>

#include "FileSystemView.h"
#include "model/FileSystemModel.h"


class FileWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileWidget(QWidget* parent = nullptr);

    signals:
        void fileSelected(const QString& path);

private:
    FileSystemModel* fileSystemModel;
    FileSystemView* fileSystemView;
};

#endif // EPISTEME_FILEWIDGET_H