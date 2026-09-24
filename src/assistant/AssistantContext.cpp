#include "../../include/assistant/AssistantContext.h"

#include "../../include/app/DocumentManager.h"
#include "../../include/text/TextEdit.h"
#include "TextDocument.h"

#include <QFileInfo>
#include <QTextCursor>

#include <functional>

AssistantContext AssistantContext::snapshot(DocumentManager *documents,
                                            TextEdit *editor,
                                            const QStringList &activity,
                                            const QString &lastUser,
                                            const QString &lastReply) {
  AssistantContext context;

  context.now = QDateTime::currentDateTimeUtc();

  if (documents) {
    if (TextDocument *document = documents->currentDocument()) {
      context.openFilePath = document->filePath();
    }
  }

  if (editor && !context.openFilePath.isEmpty()) {
    // Best-effort scope identification. If the editor is not a
    // TextEdit with a structured document, leave the scope empty.
    if (auto *document =
            qobject_cast<TextDocument *>(editor->document())) {
      const QTextCursor cursor = editor->textCursor();
      const int position = cursor.position();

      document->rebuildStructure();

      const DocumentStructure &structure = document->structure();

      // Walk the tree and find the innermost node that contains the
      // cursor. If nothing matches, leave the scope empty.
      std::function<QString(const DocumentNode &, int, int)> walk =
          [&](const DocumentNode &node, int start, int end) -> QString {
        const int nodeStart = qMax(start, node.start);
        const int nodeEnd = qMin(end, node.end);

        if (position < nodeStart || position > nodeEnd) {
          return {};
        }

        for (const DocumentNode &child : node.children) {
          const QString deeper = walk(child, nodeStart, nodeEnd);
          if (!deeper.isEmpty()) {
            return deeper;
          }
        }

        return node.id;
      };

      context.cursorScopeId = walk(structure.root(), 0,
                                   document->toPlainText().size());
    }
  }

  // Trim the activity list to the last six entries. Anything older is
  // not useful for the current moment.
  const int keep = qMin(activity.size(), 6);
  for (int i = activity.size() - keep; i < activity.size(); ++i) {
    if (i >= 0) {
      context.recentActivity.append(activity.at(i));
    }
  }

  context.lastUserMessage = lastUser.left(200);
  context.lastAssistantReply = lastReply.left(200);

  return context;
}

QString AssistantContext::toPromptSection() const {
  QString text;

  text += QStringLiteral("Current moment:\n");

  if (!openFilePath.isEmpty()) {
    text += QStringLiteral("  Open file: ")
            + QFileInfo(openFilePath).fileName() + QLatin1Char('\n');
  } else {
    text += QStringLiteral("  No file open.\n");
  }

  if (!cursorScopeId.isEmpty()) {
    text += QStringLiteral("  Cursor scope: ") + cursorScopeId
            + QLatin1Char('\n');
  }

  if (!recentActivity.isEmpty()) {
    text += QStringLiteral("  Recent activity:\n");
    for (const QString &line : recentActivity) {
      text += QStringLiteral("    - ") + line + QLatin1Char('\n');
    }
  }

  if (!lastUserMessage.isEmpty()) {
    text += QStringLiteral("  Last user message: ") + lastUserMessage
            + QLatin1Char('\n');
  }

  if (!lastAssistantReply.isEmpty()) {
    text += QStringLiteral("  Your last reply: ") + lastAssistantReply
            + QLatin1Char('\n');
  }

  return text;
}