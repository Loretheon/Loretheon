#pragma once

#include <QJsonArray>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;

class TranscriptEditPlanCard : public QWidget {
  Q_OBJECT

public:
  TranscriptEditPlanCard(const QString &planId,
                         const QString &filePath,
                         const QString &instruction,
                         const QJsonArray &commands,
                         const QString &status,
                         const QString &result,
                         QWidget *parent = nullptr);

  void setEditAccepted(int editId, bool accepted);
  void setPlanStatus(const QString &status, const QString &result);

  signals:
    void editAccepted(const QString &planId, int editId);
  void editRejected(const QString &planId, int editId);
  void applyRequested(const QString &planId);
  void cancelRequested(const QString &planId);

private:
  struct EditRow {
    int id = 0;
    QWidget *widget = nullptr;
    QLabel *statusLabel = nullptr;
    QPushButton *accept = nullptr;
    QPushButton *reject = nullptr;
    bool accepted = true;
    QString fullTitle;
  };

  void rebuildRows();
  void updateFooter();
  void updateRowControls();

  bool isDecidable() const;

  QString m_planId;
  QString m_filePath;
  QString m_instruction;
  QJsonArray m_commands;
  QString m_status;
  QString m_result;

  QLabel *m_headerLabel = nullptr;
  QLabel *m_instructionLabel = nullptr;
  QLabel *m_statusLabel = nullptr;
  QVBoxLayout *m_rowsLayout = nullptr;
  QWidget *m_rowsHost = nullptr;
  QPushButton *m_applyButton = nullptr;
  QPushButton *m_cancelButton = nullptr;
  QLabel *m_resultLabel = nullptr;

  QList<EditRow> m_rows;
};