#pragma once

#include "EditMatch.h"

#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QHBoxLayout;
class QLineEdit;
class QPushButton;
class QTextEdit;

class InferenceService;
class EditSession;
class TextEdit;

class ChatWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatWidget(
        InferenceService *inferenceService,
        EditSession *editSession,
        QWidget *parent = nullptr);

    void setActiveEditor(
        TextEdit *editor);

    void submitTranscribedText(
        const QString &text);

private slots:
    void onSendClicked();

    void onLlmDelta(
        const QString &text);

    void onLlmFinished();

    void onLlmError(
        const QString &error);

    void onEditCandidatesReady(
        const QVector<EditMatch> &candidates);

    void onEditApplied(
        bool fuzzy,
        int editDistance);

    void onEditFailed(
        const QString &reason);

    void onEditAborted();

private:
    void sendPrompt(
        const QString &prompt);

    bool takeNextJsonObject(
        QString &buffer,
        QString &objectText);

    void processEditStream();

    void appendUserMessage(
        const QString &text);

    void appendAssistantChunk(
        const QString &text);

    void appendStatusMessage(
        const QString &text);

    void renderLastAssistantMessage();

    InferenceService *m_inferenceService =
        nullptr;

    EditSession *m_editSession =
        nullptr;

    TextEdit *m_activeEditor =
        nullptr;

    QTextEdit *m_transcript =
        nullptr;

    QLineEdit *m_input =
        nullptr;

    QPushButton *m_sendButton =
        nullptr;

    QCheckBox *m_editModeCheckbox =
        nullptr;

    QVBoxLayout *m_layout =
        nullptr;

    QString m_streamingResponse;

    int m_streamingEditCount =
        0;

    bool m_assistantMessageOpen =
        false;

    bool m_awaitingEdit =
        false;

    bool m_editGenerationStopped =
        false;

    bool m_editAbortRequested =
        false;

    QElapsedTimer m_editTimer;
};