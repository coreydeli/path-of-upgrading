// SPDX-License-Identifier: MIT
#include "platform/singleinstance.h"

#ifdef Q_OS_WIN
#include "platform/localinstance.h"

#include <QCoreApplication>
#include <windows.h>
#else
#include <KDBusService>
#endif

namespace pou {
namespace {

#ifdef Q_OS_WIN
// Through a local socket, as Windows has no session bus.
class LocalSingleInstance : public SingleInstance
{
public:
    LocalSingleInstance()
        : m_instance(QCoreApplication::applicationName() + u'-' + qEnvironmentVariable("USERNAME"))
    {
        connect(&m_instance, &LocalInstance::startedAgain, this, &SingleInstance::startedAgain);
        // Windows only lets the program you started bring a window to the
        // front, so this lets the first copy show its window for it.
        AllowSetForegroundWindow(ASFW_ANY);
        if (!m_instance.claim(QCoreApplication::arguments()))
            std::exit(0);
    }

private:
    LocalInstance m_instance;
};
#else
// Through D-Bus, where the desktop can ask for the window too.
class DBusSingleInstance : public SingleInstance
{
public:
    DBusSingleInstance()
    {
        connect(&m_service, &KDBusService::activateRequested, this, &SingleInstance::startedAgain);
    }

private:
    KDBusService m_service{KDBusService::Unique};
};
#endif

} // namespace

std::unique_ptr<SingleInstance> runOnce()
{
#ifdef Q_OS_WIN
    return std::make_unique<LocalSingleInstance>();
#else
    return std::make_unique<DBusSingleInstance>();
#endif
}

} // namespace pou
