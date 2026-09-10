#include "EditApplier.h"

#include <QTextCursor>

#include <algorithm>

EditApplier::EditApplier(QObject *parent)
    : QObject(parent) {
}

bool EditApplier::apply(
    QTextDocument &document,
    const EditCommand &command,
    const EditMatch &match) {
    if (!command.isValid()) {
        const QString reason =
            QStringLiteral("Invalid edit command.");

        emit failed(reason);

        return false;
    }

    if (!match.isValid()) {
        const QString reason =
            QStringLiteral("Invalid edit match.");

        emit failed(reason);

        return false;
    }

    QString reason;

    if (!applyOne(
            document,
            command,
            match,
            reason)) {
        emit failed(reason);

        return false;
    }

    emit applied(
        match.editDistance > 0,
        match.editDistance);

    return true;
}

bool EditApplier::applyBatch(
    QTextDocument &document,
    const QVector<EditCommand> &commands,
    const QVector<EditMatch> &matches) {
    if (commands.isEmpty()) {
        const QString reason =
            QStringLiteral("Edit batch is empty.");

        emit failed(reason);

        return false;
    }

    if (commands.size() != matches.size()) {
        const QString reason =
            QStringLiteral(
                "Edit batch command/match count mismatch.");

        emit failed(reason);

        return false;
    }

    QVector<BatchEdit> edits;

    edits.reserve(
        commands.size());

    for (int i = 0;
         i < commands.size();
         ++i) {
        const EditCommand &command =
            commands.at(i);

        const EditMatch &match =
            matches.at(i);

        if (!command.isValid()) {
            const QString reason =
                QStringLiteral(
                    "Edit %1 has an invalid command.")
                    .arg(i + 1);

            emit failed(reason);

            return false;
        }

        if (!match.isValid()) {
            const QString reason =
                QStringLiteral(
                    "Edit %1 has an invalid match.")
                    .arg(i + 1);

            emit failed(reason);

            return false;
        }

        edits.append(
            BatchEdit{
                i,
                command,
                match
            });
    }

    /*
     * Validate every edit against the current document
     * before changing anything.
     *
     * Once validation starts applying edits, QTextDocument
     * offsets shift. Therefore all matches above must refer
     * to the same pre-edit document state.
     */

    for (const BatchEdit &edit :
         edits) {
        const int start =
            edit.match.start;

        const int end =
            edit.match.end;

        if (start < 0 ||
            end < start ||
            end > document.toPlainText().size()) {
            const QString reason =
                QStringLiteral(
                    "Edit %1 has an out-of-range match.")
                    .arg(edit.commandIndex + 1);

            emit failed(reason);

            return false;
        }

        if (edit.command.operation ==
            EditCommand::Operation::Insert) {
            continue;
        }

        const QString currentText =
            document.toPlainText().mid(
                start,
                end - start);

        if (currentText !=
            edit.match.matchedText) {
            const QString reason =
                QStringLiteral(
                    "Edit %1 became invalid before application.")
                    .arg(edit.commandIndex + 1);

            emit failed(reason);

            return false;
        }
    }

    sortBatch(edits);

    int maximumEditDistance = 0;
    bool usedFuzzy = false;

    for (const BatchEdit &edit :
         edits) {
        QString reason;

        if (!applyOne(
                document,
                edit.command,
                edit.match,
                reason)) {
            const QString fullReason =
                QStringLiteral(
                    "Edit %1 failed: %2")
                    .arg(edit.commandIndex + 1)
                    .arg(reason);

            emit failed(fullReason);

            return false;
        }

        maximumEditDistance =
            qMax(
                maximumEditDistance,
                edit.match.editDistance);

        if (edit.match.editDistance > 0) {
            usedFuzzy = true;
        }
    }

    emit applied(
        usedFuzzy,
        maximumEditDistance);

    return true;
}

bool EditApplier::applyOne(
    QTextDocument &document,
    const EditCommand &command,
    const EditMatch &match,
    QString &reason) {
    const QString documentText =
        document.toPlainText();

    if (match.start < 0 ||
        match.end < match.start ||
        match.end > documentText.size()) {
        reason =
            QStringLiteral(
                "Match range is outside the document.");

        return false;
    }

    QTextCursor cursor(
        &document);

    cursor.setPosition(
        match.start);

    switch (command.operation) {
    case EditCommand::Operation::Insert: {
        if (match.start != match.end) {
            reason =
                QStringLiteral(
                    "Insert match must have zero length.");

            return false;
        }

        cursor.insertText(
            command.newString);

        return true;
    }

    case EditCommand::Operation::Delete:
    case EditCommand::Operation::Replace:
        break;
    }

    const QString selectedText =
        documentText.mid(
            match.start,
            match.end - match.start);

    if (selectedText !=
        match.matchedText) {
        reason =
            QStringLiteral(
                "Matched document text changed.");

        return false;
    }

    cursor.setPosition(
        match.start);

    cursor.setPosition(
        match.end,
        QTextCursor::KeepAnchor);

    cursor.removeSelectedText();

    if (command.operation ==
        EditCommand::Operation::Replace) {
        cursor.insertText(
            command.newString);
    }

    return true;
}

void EditApplier::sortBatch(
    QVector<BatchEdit> &edits) {
    /*
     * Apply edits from the end of the document toward
     * the beginning so earlier offsets remain valid.
     *
     * For multiple insertions at the exact same position,
     * reverse command order preserves the model's array
     * order in the resulting document.
     */

    std::stable_sort(
        edits.begin(),
        edits.end(),
        [](const BatchEdit &a,
           const BatchEdit &b) {
            if (a.match.start !=
                b.match.start) {
                return a.match.start >
                       b.match.start;
            }

            if (a.match.end !=
                b.match.end) {
                return a.match.end >
                       b.match.end;
            }

            return a.commandIndex >
                   b.commandIndex;
        });
}