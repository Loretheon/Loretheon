#pragma once

#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;

// A minimal prompt dialog. Shown when the user types "@Lore" in a
// document. The dialog takes a single line of text, returns it via
// accepted(), and closes.
class LoreInputDialog : public QDialog {
  Q_OBJECT

public:
  explicit LoreInputDialog(const QPoint &globalPosition,
                           QWidget *parent = nullptr);
  ~LoreInputDialog() override;

  QString request() const;

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void showEvent(QShowEvent *event) override;

private slots:
  void onAccepted();

private:
  QLineEdit *m_input = nullptr;
  QLabel *m_hint = nullptr;
};