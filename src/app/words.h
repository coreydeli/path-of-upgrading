// SPDX-License-Identifier: MIT
#pragma once

#include "core/slots.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

// The app's names for things as the windows write them, where the saved name
// differs from the one the game uses.
class Words : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // A slot as it reads on screen, from the name it is saved under.
    Q_INVOKABLE QString slot(const QString &name) const { return slotLabel(name); }
};

} // namespace pou
