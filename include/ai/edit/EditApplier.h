#pragma once

#include "EditCommand.h"
#include "EditMatch.h"

#include <QObject>

class QTextDocument;

class EditApplier : public QObject {
  Q_OBJECT

public:
  explicit EditApplier(QObject *parent = nullptr);

  bool apply(QTextDocument &document, const EditCommand &command,
             const EditMatch &match);

signals:
  void applied(bool fuzzy, int editDistance);
  void failed(const QString &reason);
};