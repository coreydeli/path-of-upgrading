// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QStringList>
#include <memory>

namespace pou {

// The one copy of the program that runs. Started again, the new copy hands
// its arguments to this one and exits.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

Q_SIGNALS:
    // With the arguments the program was started with again. There are none
    // when the desktop asks for the window.
    void startedAgain(const QStringList &arguments);
};

// Exits when a copy is running already, once that one has been told. The
// application's name and organization domain have to be set first.
std::unique_ptr<SingleInstance> runOnce();

} // namespace pou
