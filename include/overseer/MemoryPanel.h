#pragma once

#include <QStringList>
#include <QWidget>

class MemoryFactCard;
class QVBoxLayout;
class QScrollArea;
class QPlainTextEdit;
class QLineEdit;

class MemoryPanel : public QWidget {
  Q_OBJECT

public:
  explicit MemoryPanel(QWidget *parent = nullptr);

  void loadFromFile(const QString &memoryPath);
  void saveToFile();

  QStringList facts() const { return m_facts; }
  QString prose() const;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  signals:
    void changed();

private slots:
  void onAddFactClicked();
  void onCommitAddFact();

  void onFactEdited(MemoryFactCard *card, const QString &newFact);
  void onFactRemoved(MemoryFactCard *card);
  void onFactMoveTop(MemoryFactCard *card);
  void onFactMoveBottom(MemoryFactCard *card);
  void onProseEdited();

private:
  void rebuildCards();
  void ensureSaveButtonState();

  QScrollArea *m_scroll = nullptr;
  QWidget *m_host = nullptr;
  QVBoxLayout *m_hostLayout = nullptr;

  QPlainTextEdit *m_proseEdit = nullptr;

  QLineEdit *m_newFactEdit = nullptr;

  QString m_memoryPath;
  QStringList m_facts;
};