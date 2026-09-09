#include "ChatWidget.h"

#include "EditGrammar.h"
#include "edit/EditSession.h"
#include "../text/TextEdit.h"

#include "inference/InferenceService.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <QDebug>

ChatWidget::ChatWidget(
    InferenceService *inferenceService,
    EditSession *editSession,
    QWidget *parent
)
    : QWidget(parent)
    , m_inferenceService(inferenceService)
    , m_editSession(editSession)
{
    m_transcript = new QTextEdit(this);
    m_transcript->setReadOnly(true);

    m_input = new QLineEdit(this);

    m_sendButton = new QPushButton(tr("Send"), this);

    m_editModeCheckbox = new QCheckBox(tr("Edit document"), this);

    auto *controlsLayout = new QHBoxLayout;
    controlsLayout->addWidget(m_input);
    controlsLayout->addWidget(m_editModeCheckbox);
    controlsLayout->addWidget(m_sendButton);

    m_layout = new QVBoxLayout(this);
    m_layout->addWidget(m_transcript);
    m_layout->addLayout(controlsLayout);

    setLayout(m_layout);

    connect(
        m_sendButton,
        &QPushButton::clicked,
        this,
        &ChatWidget::onSendClicked
    );

    connect(
        m_input,
        &QLineEdit::returnPressed,
        this,
        &ChatWidget::onSendClicked
    );

    if (m_inferenceService) {
        connect(
            m_inferenceService,
            &InferenceService::llmDelta,
            this,
            &ChatWidget::onLlmDelta
        );

        connect(
            m_inferenceService,
            &InferenceService::llmFinished,
            this,
            &ChatWidget::onLlmFinished
        );

        connect(
            m_inferenceService,
            &InferenceService::llmError,
            this,
            &ChatWidget::onLlmError
        );
    }

    if (m_editSession) {
        connect(
            m_editSession,
            &EditSession::candidatesReady,
            this,
            &ChatWidget::onEditCandidatesReady
        );

        connect(
            m_editSession,
            &EditSession::applied,
            this,
            &ChatWidget::onEditApplied
        );

        connect(
            m_editSession,
            &EditSession::failed,
            this,
            &ChatWidget::onEditFailed
        );

        connect(
            m_editSession,
            &EditSession::aborted,
            this,
            &ChatWidget::onEditAborted
        );
    }
}

void ChatWidget::setActiveEditor(TextEdit *editor)
{
    m_activeEditor = editor;

    if (m_editSession) {
        m_editSession->setEditor(editor);
    }
}

void ChatWidget::submitTranscribedText(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return;
    }

    m_input->setText(text);
    sendPrompt(text);
}

void ChatWidget::onSendClicked()
{
    const QString prompt = m_input->text().trimmed();

    if (prompt.isEmpty()) {
        return;
    }

    m_input->clear();
    sendPrompt(prompt);
}

void ChatWidget::sendPrompt(const QString &prompt)
{
    if (!m_inferenceService) {
        appendStatusMessage(tr("Inference service is unavailable."));
        return;
    }

    const bool editMode =
        m_editModeCheckbox &&
        m_editModeCheckbox->isChecked();

    /*
     * Cancel any previous edit session before starting a new request.
     */
    if (m_editSession) {
        m_editSession->abort();
    }

    /*
     * Reset streamed-response state.
     */
    m_streamingResponse.clear();
    m_streamingEditCount = 0;
    m_assistantMessageOpen = false;
    m_awaitingEdit = editMode;
    m_editGenerationStopped = false;
    m_editAbortRequested = false;

    appendUserMessage(prompt);

    QJsonArray messages;

    if (editMode) {
        if (!m_activeEditor) {
            appendStatusMessage(tr("No active document."));
            m_awaitingEdit = false;
            return;
        }

        const QString selectedText =
            m_activeEditor->textCursor().selectedText();

        const QString documentText =
            selectedText.isEmpty()
                ? m_activeEditor->toPlainText()
                : selectedText;

        /*
         * The model is allowed to emit any number of edit objects,
         * but it must emit only the edits actually requested by
         * the user. It must not reinterpret a single replacement as
         * a sequence of transformations.
         */
        const QString systemPrompt = QStringLiteral(
            "You are editing a text document.\n"
            "\n"
            "Respond only with one or more JSON edit objects, one after another.\n"
            "Do not use a JSON array.\n"
            "Emit each object on its own line when possible.\n"
            "Do not use Markdown or code fences.\n"
            "Do not use replace_all.\n"
            "\n"
            "Each object has exactly this form:\n"
            "{\"old_string\":\"...\",\"new_string\":\"...\"}\n"
            "\n"
            "Perform exactly the changes requested by the user.\n"
            "Do not make additional changes.\n"
            "Do not turn one requested replacement into a sequence of other "
            "replacements.\n"
            "If the user asks for one edit, normally emit one edit object.\n"
            "If the user asks for multiple distinct edits, emit one object "
            "for each requested edit.\n"
            "\n"
            "old_string identifies the exact document text to replace.\n"
            "Use the smallest amount of surrounding document text that "
            "uniquely identifies the intended target.\n"
            "Do not guess which repeated occurrence the user means.\n"
        );

        messages.append(
            QJsonObject{
                {QStringLiteral("role"), QStringLiteral("system")},
                {QStringLiteral("content"), systemPrompt}
            }
        );

        const QString userContent =
            QStringLiteral("User request:\n%1\n\nDocument:\n%2")
                .arg(prompt, documentText);

        messages.append(
            QJsonObject{
                {QStringLiteral("role"), QStringLiteral("user")},
                {QStringLiteral("content"), userContent}
            }
        );

        m_inferenceService->sendChatRequest(
            messages,
            QString(),
            0.0,
            120000,
            EditGrammar::gbnf()
        );

        return;
    }

    /*
     * Normal chat mode.
     */
    messages.append(
        QJsonObject{
            {QStringLiteral("role"), QStringLiteral("user")},
            {QStringLiteral("content"), prompt}
        }
    );

    m_inferenceService->sendChatRequest(
        messages,
        QString(),
        0.7,
        120000
    );
}

void ChatWidget::onLlmDelta(const QString &text)
{
    qDebug() << "LLM DELTA:" << text;

    if (text.isEmpty()) {
        return;
    }

    /*
     * Always show exactly what the model is producing.
     *
     * This is intentionally done before edit parsing so the user can
     * see the raw streamed response rather than a "Generating edit..."
     * placeholder.
     */
    appendAssistantChunk(text);

    if (!m_awaitingEdit) {
        return;
    }

    /*
     * When an edit has become ambiguous we have already asked the
     * inference service to abort. Any queued delta that arrives after
     * that point is still displayed, but must never be interpreted
     * as another edit.
     */
    if (m_editGenerationStopped) {
        return;
    }

    m_streamingResponse += text;

    processEditStream();
}

bool ChatWidget::takeNextJsonObject(
    QString &buffer,
    QString &objectText
)
{
    objectText.clear();

    /*
     * Skip whitespace and optional separators from the previous object.
     *
     * We deliberately accept commas here even though the preferred
     * protocol is newline-delimited JSON. This makes the stream robust
     * against models that produce:
     *
     *   {"...":"..."},
     *   {"...":"..."}
     */
    while (!buffer.isEmpty()) {
        const QChar ch = buffer.at(0);

        if (ch.isSpace() || ch == QChar(',')) {
            buffer.remove(0, 1);
            continue;
        }

        break;
    }

    if (buffer.isEmpty()) {
        return false;
    }

    /*
     * Strip an opening Markdown fence defensively.
     *
     * The system prompt explicitly forbids fences, but accepting one
     * here costs very little and prevents the parser getting stuck if
     * the model ignores that instruction.
     */
    if (buffer.startsWith(QStringLiteral("```"))) {
        const int newlineIndex = buffer.indexOf(QChar('\n'));

        if (newlineIndex < 0) {
            return false;
        }

        buffer.remove(0, newlineIndex + 1);

        while (!buffer.isEmpty() && buffer.at(0).isSpace()) {
            buffer.remove(0, 1);
        }

        if (buffer.isEmpty()) {
            return false;
        }
    }

    if (buffer.at(0) != QChar('{')) {
        /*
         * There is unexpected non-JSON text at the front.
         *
         * Do not consume arbitrary text aggressively; wait for more
         * data unless we can safely find the beginning of an object.
         */
        const int objectStart = buffer.indexOf(QChar('{'));

        if (objectStart < 0) {
            return false;
        }

        if (objectStart > 0) {
            buffer.remove(0, objectStart);
        }
    }

    if (buffer.isEmpty() || buffer.at(0) != QChar('{')) {
        return false;
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (int i = 0; i < buffer.size(); ++i) {
        const QChar ch = buffer.at(i);

        if (inString) {
            if (escaped) {
                escaped = false;
                continue;
            }

            if (ch == QChar('\\')) {
                escaped = true;
                continue;
            }

            if (ch == QChar('"')) {
                inString = false;
            }

            continue;
        }

        if (ch == QChar('"')) {
            inString = true;
            continue;
        }

        if (ch == QChar('{')) {
            ++depth;
            continue;
        }

        if (ch == QChar('}')) {
            --depth;

            if (depth == 0) {
                objectText = buffer.left(i + 1);
                buffer.remove(0, i + 1);
                return true;
            }
        }
    }

    /*
     * We have a partial JSON object. Leave it in the buffer until the
     * next streamed delta arrives.
     */
    return false;
}

void ChatWidget::processEditStream()
{
    if (!m_editSession) {
        return;
    }

    if (m_editGenerationStopped) {
        return;
    }

    for (;;) {
        QString objectText;

        if (!takeNextJsonObject(
                m_streamingResponse,
                objectText)) {
            return;
        }

        QJsonParseError parseError;

        const QJsonDocument document =
            QJsonDocument::fromJson(
                objectText.toUtf8(),
                &parseError
            );

        if (parseError.error != QJsonParseError::NoError ||
            !document.isObject()) {

            appendStatusMessage(
                tr("Invalid edit object: %1")
                    .arg(parseError.errorString())
            );

            /*
             * Do not keep trying to interpret garbage as edits.
             */
            return;
        }

        const QJsonObject object = document.object();

        const QJsonValue oldValue =
            object.value(QStringLiteral("old_string"));

        const QJsonValue newValue =
            object.value(QStringLiteral("new_string"));

        if (!oldValue.isString() || !newValue.isString()) {
            appendStatusMessage(
                tr("Edit object is missing old_string or new_string.")
            );

            continue;
        }

        EditCommand command;
        command.oldString = oldValue.toString();
        command.newString = newValue.toString();

        ++m_streamingEditCount;

        appendStatusMessage(
            tr("Edit %1 received — locating text...")
                .arg(m_streamingEditCount)
        );

        m_editSession->propose(command);

        /*
         * The session has found multiple matches and is now waiting
         * for the user to choose the intended location.
         *
         * This is the critical safety boundary:
         *
         *   1. stop consuming further edit objects
         *   2. abort the LLM generation
         *   3. discard the parser buffer
         *
         * The user must resolve the ambiguity before generation can
         * continue.
         */
        if (m_editSession->state() ==
            EditSession::State::AwaitingSelection) {

            m_editGenerationStopped = true;
            m_editAbortRequested = true;

            /*
             * The visible transcript is already independent of this
             * buffer, so it is safe to discard anything that followed
             * the ambiguous object.
             */
            m_streamingResponse.clear();

            m_inferenceService->abortChatRequest();

            appendStatusMessage(
                tr("Generation stopped — choose the intended location.")
            );

            return;
        }
    }
}

void ChatWidget::onLlmFinished()
{
    const bool waitingForSelection =
        m_editSession &&
        m_editSession->state() ==
            EditSession::State::AwaitingSelection;

    if (m_awaitingEdit) {
        /*
         * If the user is choosing among candidates, the model stream
         * has intentionally ended. This is not an incomplete-edit
         * error.
         */
        if (!waitingForSelection &&
            !m_editGenerationStopped) {

            QString remaining = m_streamingResponse;

            while (!remaining.isEmpty() &&
                   remaining.at(0).isSpace()) {
                remaining.remove(0, 1);
            }

            if (remaining.endsWith(QStringLiteral("```"))) {
                remaining.chop(3);

                while (!remaining.isEmpty() &&
                       remaining.at(0).isSpace()) {
                    remaining.remove(0, 1);
                }
            }

            if (!remaining.isEmpty()) {
                appendStatusMessage(
                    tr("Edit stream ended with incomplete JSON.")
                );
            }

            if (m_streamingEditCount == 0 &&
                remaining.isEmpty()) {

                appendStatusMessage(
                    tr("Model did not produce an edit.")
                );
            }
        }

        /*
         * If the edit is currently waiting for the user's choice,
         * EditSession remains responsible for that state.
         */
        m_awaitingEdit = false;
    }

    /*
     * A completed normal chat response is rendered as usual.
     */
    if (m_assistantMessageOpen) {
        renderLastAssistantMessage();
    }

    m_assistantMessageOpen = false;
    m_streamingResponse.clear();
    m_streamingEditCount = 0;
    m_editGenerationStopped = false;
    m_editAbortRequested = false;
}

void ChatWidget::onLlmError(const QString &error)
{
    /*
     * Aborting because the edit became ambiguous is expected control
     * flow, not an inference failure.
     */
    if (m_editAbortRequested) {
        return;
    }

    appendStatusMessage(
        tr("LLM error: %1").arg(error)
    );

    if (m_editSession) {
        m_editSession->abort();
    }

    m_streamingResponse.clear();
    m_streamingEditCount = 0;
    m_assistantMessageOpen = false;
    m_awaitingEdit = false;
    m_editGenerationStopped = false;
    m_editAbortRequested = false;
}

void ChatWidget::onEditCandidatesReady(
    const QVector<EditMatch> &candidates,
    bool fuzzy
)
{
    Q_UNUSED(fuzzy);

    appendStatusMessage(
        tr("Multiple matches found — choose the intended location (%1 candidates).")
            .arg(candidates.size())
    );
}

void ChatWidget::onEditApplied(
    bool fuzzy,
    int editDistance
)
{
    if (fuzzy) {
        appendStatusMessage(
            tr("Edit applied using fuzzy matching (distance %1).")
                .arg(editDistance)
        );
    } else {
        appendStatusMessage(
            tr("Edit applied.")
        );
    }
}

void ChatWidget::onEditFailed(const QString &reason)
{
    appendStatusMessage(
        tr("Edit failed: %1").arg(reason)
    );
}

void ChatWidget::onEditAborted()
{
    appendStatusMessage(
        tr("Edit aborted.")
    );
}

void ChatWidget::appendUserMessage(const QString &text)
{
    if (!m_transcript) {
        return;
    }

    m_transcript->append(
        QStringLiteral("<b>You:</b><br>%1")
            .arg(text.toHtmlEscaped())
    );
}

void ChatWidget::appendAssistantChunk(const QString &text)
{
    if (!m_transcript) {
        return;
    }

    if (!m_assistantMessageOpen) {
        m_transcript->append(
            QStringLiteral("<b>Assistant:</b>")
        );

        m_assistantMessageOpen = true;
    }

    /*
     * Keep streamed output visible as it arrives.
     *
     * QTextEdit::append() creates a paragraph, which is not ideal for
     * token streaming, so insert directly into the current document.
     */
    QTextCursor cursor = m_transcript->textCursor();
    cursor.movePosition(QTextCursor::End);

    cursor.insertText(text);

    m_transcript->setTextCursor(cursor);
    m_transcript->ensureCursorVisible();
}

void ChatWidget::appendStatusMessage(const QString &text)
{
    if (!m_transcript) {
        return;
    }

    m_transcript->append(
        QStringLiteral("<i>%1</i>")
            .arg(text.toHtmlEscaped())
    );
}

void ChatWidget::renderLastAssistantMessage()
{
    if (!m_transcript) {
        return;
    }

    /*
     * Streaming output is already displayed live, so there is nothing
     * else to generate here. Keep this hook because it is part of the
     * existing ChatWidget structure and can later be used for markdown
     * rendering after generation finishes.
     */
}