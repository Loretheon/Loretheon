#include "../../include/overseer/WorkstationBodyFactory.h"

#include "TextDocument.h"
#include "TextWidget.h"

#include "../../include/ai/edit/EditSession.h"

WorkstationBodyFactory::WorkstationBodyFactory() {
  m_defaultHint = QStringLiteral("editor");

  registerBody(QStringLiteral("editor"),
               [](TextDocument *document, EditSession *editSession,
                  QWidget *parent) -> QWidget * {
                 auto *widget = new TextWidget(parent);

                 widget->setWorkstationMode(true);
                 widget->setActiveDocument(document);

                 if (editSession) {
                   widget->setPreviewSession(editSession);
                 }

                 return widget;
               });
}

QString WorkstationBodyFactory::defaultHint() {
  return QStringLiteral("editor");
}

void WorkstationBodyFactory::registerBody(const QString &hint,
                                          BodyCreator creator) {
  if (hint.isEmpty() || !creator) {
    return;
  }

  m_creators.insert(hint, std::move(creator));
}

bool WorkstationBodyFactory::hasBody(const QString &hint) const {
  return m_creators.contains(hint);
}

QWidget *WorkstationBodyFactory::createBody(const QString &hint,
                                            TextDocument *document,
                                            EditSession *editSession,
                                            QWidget *parent) const {
  if (!document) {
    return nullptr;
  }

  auto it = m_creators.constFind(hint);

  if (it == m_creators.constEnd()) {
    it = m_creators.constFind(m_defaultHint);

    if (it == m_creators.constEnd()) {
      return nullptr;
    }
  }

  return it.value()(document, editSession, parent);
}