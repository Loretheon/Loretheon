#pragma once

#include "CardWidget.h"

class QLabel;

class OverseerReferenceCard : public CardWidget {
  Q_OBJECT

public:
  OverseerReferenceCard(const QString &relativePath,
                        const QString &notesRootPath,
                        QWidget *parent = nullptr);

  QString relativePath() const { return m_relativePath; }
  QString absolutePath() const;
  bool exists() const { return m_exists; }

  signals:
    void removed();
  void openRequested(const QString &relativePath);
  void openInNormalEditorRequested(const QString &relativePath);
  void stageRequested(const QString &relativePath);

protected:
  void populateBody(QVBoxLayout *bodyLayout) override;
  QMenu *buildContextMenu(QWidget *parent) override;
  void changeEvent(QEvent *event) override;

private:
  void refreshStatusDot();

  QString m_relativePath;
  QString m_notesRoot;
  bool m_exists = false;

  QLabel *m_pathLabel = nullptr;
};