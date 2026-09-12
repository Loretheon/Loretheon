#pragma once

#include "../edit/EditCommand.h"

#include <QJsonObject>
#include <QString>

namespace ChatWidgetSerialization {

QString operationString(const EditCommand &command);

QString positionString(const EditCommand &command);

QJsonObject editCommandToJson(const EditCommand &command);

} // namespace ChatWidgetSerialization