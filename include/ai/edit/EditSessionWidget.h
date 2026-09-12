#pragma once

#include <QHash>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QFrame;
class QLabel;
class QPushButton;
class QTextEdit;

class EditSessionWidget : public QWidget {
  Q_OBJECT

public:
  explicit EditSessionWidget(QWidget *parent = nullptr);

  void clearHistory();

  void startEdit(int editNumber, const QString &request);

  void setCommand(int editNumber, const QString &command);

  void setTarget(int editNumber, const QString &target);

  void setStatus(int editNumber, const QString &status);

  void finishEdit(int editNumber,
                  const QString &status = QStringLiteral("Completed"));

  void failEdit(int editNumber, const QString &reason);

  void abortEdit(int editNumber,
                 const QString &reason = QStringLiteral("Aborted"));

  void setPendingEditReady(int editNumber);

  void setPendingEditDecision(int editNumber, bool accepted);

  void showConflictGroup(int groupId, const QVector<int> &editNumbers);

  void clearConflictGroup(int groupId);

signals:
  void skipRequested();

  void pendingEditAccepted(int editNumber);

  void pendingEditRejected(int editNumber);

  void acceptAllPendingEditsRequested();

  void rejectAllPendingEditsRequested();

  void applyAcceptedPendingEditsRequested();

  void conflictResolved(int groupId, int keepEditNumber);

  void conflictGroupDiscarded(int groupId);

  void conflictBatchAborted();

private:
  struct EditCard {
    QWidget *widget = nullptr;
    QWidget *statusIndicator = nullptr;

    QLabel *titleLabel = nullptr;
    QLabel *statusLabel = nullptr;

    QTextEdit *requestEdit = nullptr;
    QTextEdit *commandEdit = nullptr;
    QTextEdit *targetEdit = nullptr;
    QTextEdit *resultEdit = nullptr;

    QWidget *reviewBar = nullptr;
    QPushButton *acceptButton = nullptr;
    QPushButton *rejectButton = nullptr;

    bool accepted = true;
    bool reviewVisible = false;
  };

  struct ConflictGroupUi {
    int groupId = -1;
    QVector<int> editNumbers;
    QFrame *wrapper = nullptr;
    QButtonGroup *choiceGroup = nullptr;
    QPushButton *discardButton = nullptr;
    QPushButton *abortButton = nullptr;
  };

  EditCard *createEditCard(int editNumber, const QString &request);

  EditCard *cardFor(int editNumber) const;

  void createReviewControls(EditCard *card, int editNumber);

  void updateReviewControls(EditCard *card);

  void setResultText(EditCard *card, const QString &text);

  void updateStatusAppearance(EditCard *card, const QString &status);

  void rebuildConflictWrapper(ConflictGroupUi &ui);

  QScrollArea *m_scrollArea = nullptr;
  QWidget *m_historyContainer = nullptr;
  QVBoxLayout *m_historyLayout = nullptr;

  QWidget *m_batchBar = nullptr;

  QPushButton *m_acceptAllButton = nullptr;
  QPushButton *m_rejectAllButton = nullptr;
  QPushButton *m_applyButton = nullptr;
  QPushButton *m_skipButton = nullptr;

  QHash<int, EditCard *> m_cards;
  QHash<int, ConflictGroupUi> m_conflictGroups;
};