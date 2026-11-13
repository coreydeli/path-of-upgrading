// SPDX-License-Identifier: MIT
#include "app/ownstyle.h"

#include <QQmlExtensionPlugin>
#include <QQuickStyle>

Q_IMPORT_QML_PLUGIN(PouStylePlugin)

using namespace Qt::Literals::StringLiterals;

namespace pou {

void useOwnStyle()
{
    QQuickStyle::setStyle(u"PouStyle"_s);
}

} // namespace pou
