#pragma once

#include "EditCommand.h"
#include "EditMatch.h"

#include <QObject>
#include <QTextDocument>
#include <QVector>

class EditApplier : public QObject {
  Q_OBJECT

public:
  explicit EditApplier(QObject *parent = nullptr);

  bool apply(
      QTextDocument &document,
      const EditCommand &command,
      const EditMatch &match);

  bool applyBatch(
      QTextDocument &document,
      const QVector<EditCommand> &commands,
      const QVector<EditMatch> &matches);

  signals:
      void applied(
          bool fuzzy,
          int editDistance);

  void failed(
      const QString &reason);

private:
  struct BatchEdit {
    int commandIndex = -1;
    EditCommand command;
    EditMatch match;
  };

  static bool applyOne(
      QTextDocument &document,
      const EditCommand &command,
      const EditMatch &match,
      QString &reason);

  static void sortBatch(
      QVector<BatchEdit> &edits);
};