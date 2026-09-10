#include "EditSession.h"

#include "EditApplier.h"
#include "EditCandidateView.h"

#include "../../text/TextEdit.h"
#include "../../text/model/TextDocument.h"

#include <QDebug>

EditSession::EditSession(
    TextEdit *editor,
    QObject *parent)
    : QObject(parent)
    , m_editor(editor) {
    m_applier =
        new EditApplier(this);

    m_candidateView =
        new EditCandidateView();

    connect(
        m_candidateView,
        &EditCandidateView::candidateSelected,
        this,
        &EditSession::onCandidateSelected);

    connect(
        m_applier,
        &EditApplier::applied,
        this,
        &EditSession::applied);

    connect(
        m_applier,
        &EditApplier::failed,
        this,
        &EditSession::failed);
}

void EditSession::setEditor(
    TextEdit *editor) {
    if (m_editor == editor) {
        return;
    }

    abort();

    m_editor =
        editor;
}

bool EditSession::propose(
    const EditCommand &command) {
    abort();

    if (!m_editor) {
        emit failed(
            QStringLiteral(
                "No active editor."));

        return false;
    }

    if (!command.isValid()) {
        emit failed(
            QStringLiteral(
                "Invalid edit command."));

        return false;
    }

    auto *document =
        qobject_cast<TextDocument *>(
            m_editor->document());

    if (!document) {
        emit failed(
            QStringLiteral(
                "Active editor does not use TextDocument."));

        return false;
    }

    setState(
        State::Matching);

    document->rebuildStructure();

    m_documentRevision =
        document->revision();

    m_pendingCommand =
        command;

    m_pendingCandidates.clear();

    EditMatcher::Result result;

    if (command.operation ==
        EditCommand::Operation::Insert) {
        EditMatch insertionMatch;

        if (!createInsertionMatch(
                command,
                insertionMatch)) {
            setState(
                State::Idle);

            return false;
        }

        result.candidates.append(
            insertionMatch);
    } else {
        result =
            m_matcher.find(
                *document,
                command);
    }

    if (result.candidates.isEmpty()) {
        setState(
            State::Idle);

        emit failed(
            QStringLiteral(
                "No matching text found in scope '%1'.")
            .arg(command.scopeId));

        return false;
    }

    m_pendingCandidates =
        result.candidates;

    if (m_pendingCandidates.size() == 1) {
        applyCandidate(
            m_pendingCandidates.first());

        return m_state !=
               State::Idle ||
               !m_pendingCandidates.isEmpty();
    }

    setState(
        State::AwaitingSelection);

    emit candidatesReady(
        m_pendingCandidates);

    return true;
}

bool EditSession::proposeMany(
    const QVector<EditCommand> &commands) {
    abort();

    if (!m_editor) {
        emit failed(
            QStringLiteral(
                "No active editor."));

        return false;
    }

    if (commands.isEmpty()) {
        emit failed(
            QStringLiteral(
                "Edit batch is empty."));

        return false;
    }

    auto *document =
        qobject_cast<TextDocument *>(
            m_editor->document());

    if (!document) {
        emit failed(
            QStringLiteral(
                "Active editor does not use TextDocument."));

        return false;
    }

    for (int i = 0;
         i < commands.size();
         ++i) {
        if (!commands.at(i).isValid()) {
            emit failed(
                QStringLiteral(
                    "Edit %1 is invalid.")
                .arg(i + 1));

            return false;
        }
    }

    setState(
        State::Matching);

    document->rebuildStructure();

    m_documentRevision =
        document->revision();

    QVector<EditMatch> matches;

    if (!resolveBatch(
            commands,
            matches)) {
        setState(
            State::Idle);

        return false;
    }

    /*
     * At this point every command has been resolved
     * against the same document revision.
     *
     * Do not modify the document until every edit is
     * known to be valid.
     */

    setState(
        State::Applying);

    const bool applied =
        m_applier->applyBatch(
            *document,
            commands,
            matches);

    m_pendingCandidates.clear();

    setState(
        State::Idle);

    return applied;
}

void EditSession::abort() {
    if (m_state == State::Idle) {
        m_pendingCandidates.clear();

        return;
    }

    m_pendingCandidates.clear();

    setState(
        State::Idle);

    emit aborted();
}

void EditSession::setState(
    State state) {
    if (m_state == state) {
        return;
    }

    m_state =
        state;

    emit stateChanged(
        m_state);
}

void EditSession::applyCandidate(
    const EditMatch &match) {
    if (!m_editor) {
        setState(
            State::Idle);

        emit failed(
            QStringLiteral(
                "No active editor."));

        return;
    }

    auto *document =
        qobject_cast<TextDocument *>(
            m_editor->document());

    if (!document) {
        setState(
            State::Idle);

        emit failed(
            QStringLiteral(
                "Active editor does not use TextDocument."));

        return;
    }

    if (document->revision() !=
        m_documentRevision) {
        m_pendingCandidates.clear();

        setState(
            State::Idle);

        emit failed(
            QStringLiteral(
                "Document changed before the edit was applied."));

        return;
    }

    if (!match.isValid()) {
        setState(
            State::Idle);

        emit failed(
            QStringLiteral(
                "Invalid edit match."));

        return;
    }

    setState(
        State::Applying);

    m_applier->apply(
        *document,
        m_pendingCommand,
        match);

    m_pendingCandidates.clear();

    setState(
        State::Idle);
}

bool EditSession::resolveSingle(
    const EditCommand &command,
    EditMatch &match) {
    auto *document =
        qobject_cast<TextDocument *>(
            m_editor->document());

    if (!document) {
        emit failed(
            QStringLiteral(
                "Active editor does not use TextDocument."));

        return false;
    }

    if (command.operation ==
        EditCommand::Operation::Insert) {
        return createInsertionMatch(
            command,
            match);
    }

    const EditMatcher::Result result =
        m_matcher.find(
            *document,
            command);

    if (result.candidates.isEmpty()) {
        emit failed(
            QStringLiteral(
                "No matching text found in scope '%1'.")
            .arg(command.scopeId));

        return false;
    }

    if (result.candidates.size() > 1 &&
        !command.replaceAll) {
        emit failed(
    QStringLiteral(
        "Edit has %1 possible matches in scope '%2'.")
        .arg(result.candidates.size())
        .arg(command.scopeId));

        return false;
    }

    match =
        result.candidates.first();

    return true;
}

bool EditSession::resolveBatch(
    const QVector<EditCommand> &commands,
    QVector<EditMatch> &matches) {
    auto *document =
        qobject_cast<TextDocument *>(
            m_editor->document());

    if (!document) {
        emit failed(
            QStringLiteral(
                "Active editor does not use TextDocument."));

        return false;
    }

    matches.clear();

    matches.reserve(
        commands.size());

    for (int i = 0;
         i < commands.size();
         ++i) {
        const EditCommand &command =
            commands.at(i);

        EditMatch match;

        if (!resolveSingle(
                command,
                match)) {
            emit failed(
                QStringLiteral(
                    "Edit %1 could not be resolved.")
                .arg(i + 1));

            return false;
        }

        matches.append(
            match);
    }

    if (document->revision() !=
        m_documentRevision) {
        emit failed(
            QStringLiteral(
                "Document changed while resolving edits."));

        return false;
    }

    return true;
}

bool EditSession::createInsertionMatch(
    const EditCommand &command,
    EditMatch &match) {
    auto *document =
        qobject_cast<TextDocument *>(
            m_editor->document());

    if (!document) {
        emit failed(
            QStringLiteral(
                "Active editor does not use TextDocument."));

        return false;
    }

    if (!command.findString.isEmpty()) {
        emit failed(
            QStringLiteral(
                "Insert operations must have an empty find string."));

        return false;
    }

    document->rebuildStructure();

    const DocumentStructure &structure =
        document->structure();

    const DocumentNode *scope =
        structure.find(
            command.scopeId);

    if (!scope) {
        emit failed(
            QStringLiteral(
                "Insertion scope '%1' was not found.")
            .arg(command.scopeId));

        return false;
    }

    const int scopeStart =
        scope->start;

    const int scopeEnd =
        scope->end;

    const int documentLength =
        document->toPlainText().size();

    const bool validStart =
        scopeStart >= 0 &&
        scopeStart <= documentLength;

    const bool validEnd =
        scopeEnd >= scopeStart &&
        scopeEnd <= documentLength;

    if (!validStart ||
        !validEnd) {
        emit failed(
            QStringLiteral(
                "Insertion scope '%1' has an invalid range.")
            .arg(command.scopeId));

        return false;
    }

    int position = scopeStart;

    if (command.position ==
        EditCommand::Position::After) {
        position =
            scopeEnd;
    }

    match.start =
        position;

    match.end =
        position;

    match.editDistance =
        0;

    match.matchedText.clear();

    return true;
}

void EditSession::onCandidateSelected(
    int index) {
    if (m_state !=
        State::AwaitingSelection) {
        return;
    }

    if (!m_editor) {
        abort();

        emit failed(
            QStringLiteral(
                "No active editor."));

        return;
    }

    auto *document =
        qobject_cast<TextDocument *>(
            m_editor->document());

    if (!document) {
        abort();

        emit failed(
            QStringLiteral(
                "Active editor does not use TextDocument."));

        return;
    }

    if (document->revision() !=
        m_documentRevision) {
        m_pendingCandidates.clear();

        setState(
            State::Idle);

        emit failed(
            QStringLiteral(
                "Document changed before candidate selection."));

        return;
    }

    if (index < 0 ||
        index >= m_pendingCandidates.size()) {
        emit failed(
            QStringLiteral(
                "Invalid edit candidate."));

        return;
    }

    const EditMatch match =
        m_pendingCandidates.at(index);

    applyCandidate(
        match);
}