#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QString>
#include <QVector>
#include <QWidget>

#include "edit/EditMatch.h"

class QTextEdit;
class InferenceService;
class EditSession;
class TextEdit;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QVBoxLayout;

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(
        InferenceService *inferenceService,
        EditSession *editSession,
        QWidget *parent = nullptr
    );

    void setActiveEditor(TextEdit *editor);
    void submitTranscribedText(const QString &text);

private slots:
    void onSendClicked();

    void onLlmDelta(const QString &text);
    void onLlmFinished();
    void onLlmError(const QString &error);

    void onEditCandidatesReady(
        const QVector<EditMatch> &candidates,
        bool fuzzy
    );

    void onEditApplied(
        bool fuzzy,
        int editDistance
    );

    void onEditFailed(const QString &reason);
    void onEditAborted();

private:
    void sendPrompt(const QString &prompt);

    void appendUserMessage(const QString &text);
    void appendAssistantChunk(const QString &text);
    void appendStatusMessage(const QString &text);
    void renderLastAssistantMessage();

    void processEditStream();

    static bool takeNextJsonObject(
        QString &buffer,
        QString &objectText
    );

private:
    InferenceService *m_inferenceService{nullptr};
    EditSession *m_editSession{nullptr};
    TextEdit *m_activeEditor{nullptr};

    QTextEdit *m_transcript{nullptr};
    QLineEdit *m_input{nullptr};
    QPushButton *m_sendButton{nullptr};
    QCheckBox *m_editModeCheckbox{nullptr};
    QVBoxLayout *m_layout{nullptr};

    /*
     * This is the parser buffer for the current streamed response.
     *
     * It is deliberately separate from the visible transcript.
     * Clearing this buffer when an edit becomes ambiguous does not
     * remove anything the user has already seen streaming on screen.
     */
    QString m_streamingResponse;

    int m_streamingEditCount{0};

    bool m_assistantMessageOpen{false};
    bool m_awaitingEdit{false};

    /*
     * True once we have intentionally stopped generation because
     * an edit requires user selection.
     *
     * Additional queued deltas may still arrive after abortChatRequest(),
     * so we continue displaying them but never parse them as edits.
     */
    bool m_editGenerationStopped{false};

    /*
     * Distinguishes an expected cancellation caused by our ambiguity
     * handling from a real inference error.
     */
    bool m_editAbortRequested{false};
};

#endif