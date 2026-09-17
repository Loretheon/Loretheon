#pragma once

#include "CardWidget.h"

class QLabel;
class QLineEdit;
class QToolButton;
class QPlainTextEdit;

class MemoryFactCard : public CardWidget {
  Q_OBJECT

public:
  MemoryFactCard(const QString &fact, const QString &rationale,
                 QWidget *parent = nullptr);

  QString fact() const { return m_fact; }
  QString rationale() const { return m_rationale; }

  void setFact(const QString &fact);
  void setRationale(const QString &rationale);

  void beginInlineEdit();

  signals:
    void edited(const QString &newFact);
  void removed();
  void moveToTopRequested();
  void moveToBottomRequested();

protected:
  void populateBody(QVBoxLayout *bodyLayout) override;
  QMenu *buildContextMenu(QWidget *parent) override;

private:
  void commitEdit();
  void cancelEdit();
  void refreshBodyVisibility();

  QString m_fact;
  QString m_rationale;

  QLabel *m_factLabel = nullptr;
  QLabel *m_rationaleLabel = nullptr;
  QLineEdit *m_factEdit = nullptr;

  bool m_editing = false;
};