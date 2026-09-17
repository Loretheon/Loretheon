#pragma once

#include <QStringList>
#include <QWidget>

class FileWidget;
class OverseerReferenceCard;

class QVBoxLayout;
class QScrollArea;
class QLineEdit;
class QLabel;

class OverviewPanel : public QWidget {
  Q_OBJECT

public:
  explicit OverviewPanel(QWidget *parent = nullptr);

  void setNotesRoot(const QString &notesRootPath);

  void loadFromFile(const QString &overviewPath);
  void saveToFile();

  void addRelativePath(const QString &relativePath);
  void addRelativePaths(const QStringList &relativePaths);
  void removeAllMissing();

  QStringList relativePaths() const { return m_paths; }

  signals:
    void changed();
  void openRequested(const QString &relativePath);
  void openInNormalEditorRequested(const QString &relativePath);
  void stageRequested(const QString &relativePath);
  void addFromTreeRequested();

private slots:
  void onAddPathClicked();
  void onCommitAddPath();

  void onCardRemoved(OverseerReferenceCard *card);

private:
  void rebuildCards();
  void updateMissingHeader();

  QScrollArea *m_scroll = nullptr;
  QWidget *m_host = nullptr;
  QVBoxLayout *m_hostLayout = nullptr;

  QLineEdit *m_addEdit = nullptr;
  QLabel *m_missingLabel = nullptr;

  FileWidget *m_fileWidget = nullptr;

  QString m_notesRoot;
  QString m_overviewPath;
  QStringList m_paths;
};