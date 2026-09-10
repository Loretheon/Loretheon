#include "ChatWidget.h"

#include "EditGrammar.h"
#include "edit/EditCommand.h"
#include "edit/EditSession.h"
#include "edit/EditMatch.h"

#include "../text/TextEdit.h"
#include "../text/model/TextDocument.h"

#include "inference/InferenceService.h"

#include <QCheckBox>
#include <QDebug>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLineEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

ChatWidget::ChatWidget(
    InferenceService *inferenceService,
    EditSession *editSession,
    QWidget *parent)
    : QWidget(parent)
    , m_inferenceService(inferenceService)
    , m_editSession(editSession) {
    m_transcript =
        new QTextEdit(this);

    m_transcript->setReadOnly(
        true);

    m_input =
        new QLineEdit(this);

    m_sendButton =
        new QPushButton(
            tr("Send"),
            this);

    m_editModeCheckbox =
        new QCheckBox(
            tr("Edit document"),
            this);

    auto *controlsLayout =
        new QHBoxLayout;

    controlsLayout->addWidget(
        m_input);

    controlsLayout->addWidget(
        m_editModeCheckbox);

    controlsLayout->addWidget(
        m_sendButton);

    m_layout =
        new QVBoxLayout(this);

    m_layout->addWidget(
        m_transcript);

    m_layout->addLayout(
        controlsLayout);

    setLayout(
        m_layout);

    connect(
        m_sendButton,
        &QPushButton::clicked,
        this,
        &ChatWidget::onSendClicked);

    connect(
        m_input,
        &QLineEdit::returnPressed,
        this,
        &ChatWidget::onSendClicked);

    if (m_inferenceService) {
        connect(
            m_inferenceService,
            &InferenceService::llmDelta,
            this,
            &ChatWidget::onLlmDelta);

        connect(
            m_inferenceService,
            &InferenceService::llmFinished,
            this,
            &ChatWidget::onLlmFinished);

        connect(
            m_inferenceService,
            &InferenceService::llmError,
            this,
            &ChatWidget::onLlmError);
    }

    if (m_editSession) {
        connect(
            m_editSession,
            &EditSession::candidatesReady,
            this,
            &ChatWidget::onEditCandidatesReady);

        connect(
            m_editSession,
            &EditSession::applied,
            this,
            &ChatWidget::onEditApplied);

        connect(
            m_editSession,
            &EditSession::failed,
            this,
            &ChatWidget::onEditFailed);

        connect(
            m_editSession,
            &EditSession::aborted,
            this,
            &ChatWidget::onEditAborted);
    }
}

void ChatWidget::setActiveEditor(
    TextEdit *editor) {
    m_activeEditor =
        editor;

    if (m_editSession) {
        m_editSession->setEditor(
            editor);
    }
}

void ChatWidget::submitTranscribedText(
    const QString &text) {
    if (text.trimmed().isEmpty()) {
        return;
    }

    m_input->setText(
        text);

    sendPrompt(
        text);
}

void ChatWidget::onSendClicked() {
    const QString prompt =
        m_input->text().trimmed();

    if (prompt.isEmpty()) {
        return;
    }

    m_input->clear();

    sendPrompt(
        prompt);
}

void ChatWidget::sendPrompt(
    const QString &prompt) {
    if (!m_inferenceService) {
        appendStatusMessage(
            tr(
                "Inference service is unavailable."));

        return;
    }

    const bool editMode =
        m_editModeCheckbox &&
        m_editModeCheckbox->isChecked();

    if (m_editSession) {
        m_editSession->abort();
    }

    m_streamingResponse.clear();

    m_streamingEditCount =
        0;

    m_assistantMessageOpen =
        false;

    m_awaitingEdit =
        editMode;

    m_editGenerationStopped =
        false;

    m_editAbortRequested =
        false;

    if (editMode) {
        m_editTimer.start();

        appendStatusMessage(
            tr(
                "Edit: generating changes..."));
    }

    appendUserMessage(
        prompt);

    QJsonArray messages;

    if (editMode) {
        if (!m_activeEditor) {
            appendStatusMessage(
                tr(
                    "No active document."));

            m_awaitingEdit =
                false;

            return;
        }

        auto *document =
            qobject_cast<TextDocument *>(
                m_activeEditor->document());

        if (!document) {
            appendStatusMessage(
                tr(
                    "Active editor does not use TextDocument."));

            m_awaitingEdit =
                false;

            return;
        }

        document->rebuildStructure();

        const QString structure =
            document->structure().indexForModel();

        const QString documentText =
            m_activeEditor->toPlainText();

        const QString selectedText =
            m_activeEditor
                ->textCursor()
                .selectedText();

        QString selectionContext;

        if (!selectedText.isEmpty()) {
            selectionContext =
                QStringLiteral(
                    "\nCurrent selection:\n%1\n")
                    .arg(
                        selectedText);
        }

        const QString systemPrompt =
            QStringLiteral(
                "You are editing a text document.\n"
                "\n"
                "Return edit objects one at a time.\n"
                "Each edit must be one complete JSON object.\n"
                "Output one object, then a newline, then the next object.\n"
                "Do not wrap the objects in a JSON array.\n"
                "Never return markdown fences.\n"
                "\n"
                "Every edit object MUST contain exactly these seven fields:\n"
                "operation, scope, position, find, new, all.\n"
                "\n"
                "Each edit has this form:\n"
                "{"
                "\"operation\":\"insert\","
                "\"scope\":\"document\","
                "\"position\":\"before\","
                "\"find\":\"\","
                "\"new\":\"text\","
                "\"all\":false"
                "}\n"
                "\n"
                "operation must be exactly one of: insert, replace, delete.\n"
                "scope must be copied exactly from the document structure.\n"
                "position must be exactly before or after.\n"
                "all must always be present and must be true or false.\n"
                "\n"
                "For insert:\n"
                "- find MUST be an empty string.\n"
                "\n"
                "For replace:\n"
                "- find MUST be non-empty.\n"
                "- find must be a short, distinctive piece of existing text "
                "inside the selected scope.\n"
                "\n"
                "For delete:\n"
                "- find MUST be non-empty.\n"
                "- find must be a short, distinctive piece of existing text "
                "inside the selected scope.\n"
                "\n"
                "Choose the smallest structural scope containing the change.\n"
                "Do not use character offsets.\n"
                "Do not invent scope IDs.\n"
                "Do not count paragraphs or headings yourself.\n"
                "Copy scope IDs exactly as provided.\n"
                "\n"
                "Prefer multiple small, precise edits when different parts "
                "of the document need independent changes.\n"
                "Do not replace an entire large section when smaller edits "
                "can accomplish the request safely.\n"
                "Use the fewest edits necessary.\n"
                "\n"
                "The document is Markdown. Write \"new\" text in Markdown.\n"
                "\n"
                "Edits are applied as they arrive. The document structure is "
                "updated after each edit, so unchanged Tree-sitter scopes "
                "remain usable while changed scopes may receive new IDs.\n"
                "\n"
                "Document structure:\n"
                "%1\n"
                "\n"
                "User request:\n"
                "%2\n"
                "%3\n"
                "Document:\n"
                "%4")
                .arg(
                    structure,
                    prompt,
                    selectionContext,
                    documentText);

        messages.append(
            QJsonObject{
                {
                    QStringLiteral("role"),
                    QStringLiteral("system")
                },
                {
                    QStringLiteral("content"),
                    systemPrompt
                }
            });

        m_inferenceService->sendChatRequest(
            messages,
            QString(),
            0.0,
            120000,
            EditGrammar::gbnf());

        return;
    }

    messages.append(
        QJsonObject{
            {
                QStringLiteral("role"),
                QStringLiteral("user")
            },
            {
                QStringLiteral("content"),
                prompt
            }
        });

    m_inferenceService->sendChatRequest(
        messages,
        QString(),
        0.7,
        120000);
}

void ChatWidget::onLlmDelta(
    const QString &text) {
    if (text.isEmpty()) {
        return;
    }

    if (!m_awaitingEdit) {
        appendAssistantChunk(
            text);

        return;
    }

    if (m_editGenerationStopped) {
        return;
    }

    m_streamingResponse +=
        text;

    processEditStream();
}

bool ChatWidget::takeNextJsonObject(
    QString &buffer,
    QString &objectText) {
    objectText.clear();

    while (!buffer.isEmpty() &&
           buffer.at(0).isSpace()) {
        buffer.remove(
            0,
            1);
    }

    if (buffer.isEmpty()) {
        return false;
    }

    if (buffer.at(0) !=
        QChar('{')) {
        const int objectStart =
            buffer.indexOf(
                QChar('{'));

        if (objectStart < 0) {
            return false;
        }

        buffer.remove(
            0,
            objectStart);
    }

    if (buffer.isEmpty() ||
        buffer.at(0) !=
            QChar('{')) {
        return false;
    }

    int depth = 0;

    bool inString =
        false;

    bool escaped =
        false;

    for (int i = 0;
         i < buffer.size();
         ++i) {
        const QChar ch =
            buffer.at(i);

        if (inString) {
            if (escaped) {
                escaped =
                    false;

                continue;
            }

            if (ch == QChar('\\')) {
                escaped =
                    true;

                continue;
            }

            if (ch == QChar('"')) {
                inString =
                    false;
            }

            continue;
        }

        if (ch == QChar('"')) {
            inString =
                true;

            continue;
        }

        if (ch == QChar('{')) {
            ++depth;

            continue;
        }

        if (ch == QChar('}')) {
            --depth;

            if (depth == 0) {
                objectText =
                    buffer.left(
                        i + 1);

                buffer.remove(
                    0,
                    i + 1);

                return true;
            }
        }
    }

    return false;
}

void ChatWidget::processEditStream() {
    if (!m_editSession ||
        m_editGenerationStopped) {
        return;
    }

    QString objectText;

    while (takeNextJsonObject(
        m_streamingResponse,
        objectText)) {
        QJsonParseError parseError;

        const QJsonDocument document =
            QJsonDocument::fromJson(
                objectText.toUtf8(),
                &parseError);

        if (parseError.error !=
            QJsonParseError::NoError ||
            !document.isObject()) {
            appendStatusMessage(
                tr(
                    "Edit: invalid JSON object."));

            m_editGenerationStopped =
                true;

            m_editAbortRequested =
                true;

            m_inferenceService
                ->abortChatRequest();

            return;
        }

        const QJsonObject object =
            document.object();

        const QJsonValue operationValue =
            object.value(
                QStringLiteral("operation"));

        const QJsonValue scopeValue =
            object.value(
                QStringLiteral("scope"));

        const QJsonValue positionValue =
            object.value(
                QStringLiteral("position"));

        const QJsonValue findValue =
            object.value(
                QStringLiteral("find"));

        const QJsonValue newValue =
            object.value(
                QStringLiteral("new"));

        const QJsonValue allValue =
            object.value(
                QStringLiteral("all"));

        if (!operationValue.isString() ||
            !scopeValue.isString() ||
            !positionValue.isString() ||
            !findValue.isString() ||
            !newValue.isString() ||
            !allValue.isBool()) {
            appendStatusMessage(
                tr(
                    "Edit: missing required field."));

            m_editGenerationStopped =
                true;

            m_editAbortRequested =
                true;

            m_inferenceService
                ->abortChatRequest();

            return;
        }

        EditCommand command;

        const QString operation =
            operationValue.toString();

        if (operation ==
            QStringLiteral("insert")) {
            command.operation =
                EditCommand::Operation::Insert;
        } else if (operation ==
                   QStringLiteral("replace")) {
            command.operation =
                EditCommand::Operation::Replace;
        } else if (operation ==
                   QStringLiteral("delete")) {
            command.operation =
                EditCommand::Operation::Delete;
        } else {
            appendStatusMessage(
                tr(
                    "Edit: invalid operation '%1'.")
                .arg(
                    operation));

            m_editGenerationStopped =
                true;

            m_editAbortRequested =
                true;

            m_inferenceService
                ->abortChatRequest();

            return;
        }

        const QString position =
            positionValue.toString();

        if (position ==
            QStringLiteral("before")) {
            command.position =
                EditCommand::Position::Before;
        } else if (position ==
                   QStringLiteral("after")) {
            command.position =
                EditCommand::Position::After;
        } else {
            appendStatusMessage(
                tr(
                    "Edit: invalid position '%1'.")
                .arg(
                    position));

            m_editGenerationStopped =
                true;

            m_editAbortRequested =
                true;

            m_inferenceService
                ->abortChatRequest();

            return;
        }

        command.scopeId =
            scopeValue.toString();

        command.findString =
            findValue.toString();

        command.newString =
            newValue.toString();

        command.replaceAll =
            allValue.toBool();

        if (!command.isValid()) {
            appendStatusMessage(
                tr(
                    "Edit: operation violates the edit protocol."));

            m_editGenerationStopped =
                true;

            m_editAbortRequested =
                true;

            m_inferenceService
                ->abortChatRequest();

            return;
        }

        ++m_streamingEditCount;

        QString action;

        switch (command.operation) {
        case EditCommand::Operation::Insert:
            action =
                QStringLiteral(
                    "inserting");
            break;

        case EditCommand::Operation::Replace:
            action =
                QStringLiteral(
                    "updating");
            break;

        case EditCommand::Operation::Delete:
            action =
                QStringLiteral(
                    "deleting");
            break;
        }

        appendStatusMessage(
            tr(
                "Edit %1: %2 in %3...")
            .arg(
                m_streamingEditCount)
            .arg(
                action)
            .arg(
                command.scopeId));

        if (!m_editSession->propose(
                command)) {
            m_editGenerationStopped =
                true;

            m_editAbortRequested =
                true;

            m_inferenceService
                ->abortChatRequest();

            return;
        }

        appendStatusMessage(
            tr(
                "Edit %1 applied in %2 ms.")
            .arg(
                m_streamingEditCount)
            .arg(
                m_editTimer.elapsed()));

        objectText.clear();
    }
}

void ChatWidget::onLlmFinished() {
    if (m_awaitingEdit) {
        if (!m_editGenerationStopped) {
            if (!m_streamingResponse.trimmed().isEmpty()) {
                appendStatusMessage(
                    tr(
                        "Edit: stream ended with incomplete JSON."));
            } else if (m_streamingEditCount == 0) {
                appendStatusMessage(
                    tr(
                        "Edit: model did not produce any changes."));
            } else {
                appendStatusMessage(
                    tr(
                        "Edit: completed in %1 ms.")
                        .arg(
                            m_editTimer.elapsed()));
            }
        }

        m_awaitingEdit =
            false;
    }

    if (m_assistantMessageOpen) {
        renderLastAssistantMessage();
    }

    m_assistantMessageOpen =
        false;

    m_streamingResponse.clear();

    m_streamingEditCount =
        0;

    m_editGenerationStopped =
        false;

    m_editAbortRequested =
        false;
}

void ChatWidget::onLlmError(
    const QString &error) {
    if (m_editAbortRequested) {
        return;
    }

    appendStatusMessage(
        tr(
            "LLM error: %1")
            .arg(
                error));

    if (m_editSession) {
        m_editSession->abort();
    }

    m_streamingResponse.clear();

    m_streamingEditCount =
        0;

    m_assistantMessageOpen =
        false;

    m_awaitingEdit =
        false;

    m_editGenerationStopped =
        false;

    m_editAbortRequested =
        false;
}

void ChatWidget::onEditCandidatesReady(
    const QVector<EditMatch> &candidates) {
    appendStatusMessage(
        tr(
            "Multiple matches found — choose the intended location (%1 candidates).")
            .arg(
                candidates.size()));
}

void ChatWidget::onEditApplied(
    bool fuzzy,
    int editDistance) {
    if (fuzzy) {
        appendStatusMessage(
            tr(
                "Matched using fuzzy matching (distance %1).")
                .arg(
                    editDistance));
    }
}

void ChatWidget::onEditFailed(
    const QString &reason) {
    appendStatusMessage(
        tr(
            "Edit failed: %1")
            .arg(
                reason));
}

void ChatWidget::onEditAborted() {
    appendStatusMessage(
        tr(
            "Edit aborted."));
}

void ChatWidget::appendUserMessage(
    const QString &text) {
    if (!m_transcript) {
        return;
    }

    QTextCursor cursor =
        m_transcript->textCursor();

    cursor.movePosition(
        QTextCursor::End);

    QTextCharFormat format;

    format.setFontWeight(
        QFont::Bold);

    cursor.insertText(
        QStringLiteral("You:"),
        format);

    cursor.insertText(
        QStringLiteral(" "));

    cursor.insertText(
        text);

    cursor.insertText(
        QStringLiteral("\n"));

    m_transcript->setTextCursor(
        cursor);

    m_transcript->ensureCursorVisible();
}

void ChatWidget::appendAssistantChunk(
    const QString &text) {
    if (!m_transcript) {
        return;
    }

    if (!m_assistantMessageOpen) {
        m_transcript->append(
            QStringLiteral(
                "<b>Assistant:</b>"));

        m_assistantMessageOpen =
            true;
    }

    QTextCursor cursor =
        m_transcript->textCursor();

    cursor.movePosition(
        QTextCursor::End);

    cursor.insertText(
        text);

    m_transcript->setTextCursor(
        cursor);

    m_transcript->ensureCursorVisible();
}

void ChatWidget::appendStatusMessage(
    const QString &text) {
    if (!m_transcript) {
        return;
    }

    QTextCursor cursor =
        m_transcript->textCursor();

    cursor.movePosition(
        QTextCursor::End);

    QTextCharFormat format;

    format.setFontItalic(
        true);

    cursor.insertText(
        text,
        format);

    cursor.insertText(
        QStringLiteral("\n"));

    m_transcript->setTextCursor(
        cursor);

    m_transcript->ensureCursorVisible();
}

void ChatWidget::renderLastAssistantMessage() {
    if (!m_transcript) {
        return;
    }
}