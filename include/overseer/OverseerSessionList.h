#pragma once

#include <QWidget>

class QListWidget;
class QPushButton;

class OverseerSessionList : public QWidget {
  Q_OBJECT

public:
  explicit OverseerSessionList(QWidget *parent = nullptr);

  QListWidget *list() const { return m_list; }

  void rebuild(const QStringList &names);
  void selectByName(const QString &name);

  signals:
    void newSessionRequested();
  void sessionSelected(const QString &name);
  void sessionCleared();

private slots:
  void onSelectionChanged();

private:
  QListWidget *m_list = nullptr;
  QPushButton *m_newButton = nullptr;
};