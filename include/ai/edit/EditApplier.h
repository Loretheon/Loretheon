#pragma once

#include "EditCommand.h"
#include "EditMatch.h"

#include <QObject>
#include <QTextCursor>
#include <QTextDocument>
#include <QVector>

class EditApplier : public QObject {
  Q_OBJECT

public:
  explicit EditApplier(QObject *parent = nullptr);

  bool apply(QTextDocument &document, const EditCommand &command,
             const EditMatch &match);

  bool applyBatch(QTextDocument &document, const QVector<EditCommand> &commands,
                  const QVector<EditMatch> &matches);

  bool beginStreaming(QTextDocument &document, const EditCommand &command,
                      const EditMatch &match);

  bool appendStreaming(const QString &text);

  bool finishStreaming();

  void cancelStreaming();

  bool isStreaming() const { return m_streaming; }

signals:
  void applied(bool fuzzy, int editDistance);

  void failed(const QString &reason);

private:
  struct BatchEdit {
    int commandIndex = -1;

    EditCommand command;

    EditMatch match;
  };

  static bool applyOne(QTextDocument &document, const EditCommand &command,
                       const EditMatch &match, QString &reason);
  void resetStreamingState();

  static void sortBatch(QVector<BatchEdit> &edits);

  QTextDocument *m_streamingDocument = nullptr;

  QTextCursor m_streamingCursor;

  EditCommand m_streamingCommand;

  EditMatch m_streamingMatch;

  QString m_streamingOriginalText;

  int m_streamingStart = -1;

  int m_streamingLength = 0;

  bool m_streaming = false;
};