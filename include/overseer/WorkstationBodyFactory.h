#pragma once

#include <QHash>
#include <QString>
#include <QWidget>

#include <functional>

class TextDocument;
class EditSession;

class WorkstationBodyFactory {
public:
  using BodyCreator =
      std::function<QWidget *(TextDocument *document,
                              EditSession *editSession,
                              QWidget *parent)>;

  WorkstationBodyFactory();

  static QString defaultHint();

  void registerBody(const QString &hint, BodyCreator creator);

  bool hasBody(const QString &hint) const;

  QWidget *createBody(const QString &hint, TextDocument *document,
                      EditSession *editSession, QWidget *parent) const;

private:
  QHash<QString, BodyCreator> m_creators;
  QString m_defaultHint;
};