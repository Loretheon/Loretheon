#include "../../include/file/FileWidget.h"

#include <QVBoxLayout>

#include "Settings.h"


FileWidget::FileWidget(QWidget *parent)
    : QWidget(parent)
{
    fileSystemModel = new FileSystemModel(this);
    fileSystemView = new FileSystemView(this);

    fileSystemView->setModel(fileSystemModel);

    connect(fileSystemView,
            &FileSystemView::doubleClicked,
            this,
            [this](const QModelIndex &index)
            {
                if (fileSystemModel->isDir(index))
                    return;

                emit fileSelected(fileSystemModel->filePath(index));
            });


    const QString root = Settings::getRootDirectory();

    const QModelIndex rootIndex = fileSystemModel->setRootPath(root);
    fileSystemView->setRootIndex(rootIndex);


    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(fileSystemView);
}