// SPDX-License-Identifier: MIT
#include "platform/kde/kdeglobalshortcuts.h"

#include <KGlobalAccel>
#include <QAction>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QGuiApplication>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Named after the app's desktop file, System Settings shows them under the
// app's own name and icon.
QString component()
{
    const QString desktopFile = QGuiApplication::desktopFileName();
    return desktopFile.isEmpty() ? QCoreApplication::applicationName() : desktopFile;
}

} // namespace

KdeGlobalShortcuts::KdeGlobalShortcuts(QObject *parent)
    : GlobalShortcuts(parent)
{
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this,
            [this](QAction *action, const QKeySequence &key) {
                if (m_actions.value(action->objectName()) == action)
                    Q_EMIT keyChanged(action->objectName(), key);
            });
}

bool KdeGlobalShortcuts::isAvailable() const
{
    const QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    return bus && bus->isServiceRegistered(u"org.kde.kglobalaccel"_s);
}

QString KdeGlobalShortcuts::errorString() const
{
    if (isAvailable())
        return {};
    return u"KDE's global shortcuts service isn't running, so the hotkeys won't work. It comes with Plasma."_s;
}

void KdeGlobalShortcuts::add(const QString &id, const QString &name, const QKeySequence &key)
{
    auto *action = new QAction(name, this);
    action->setObjectName(id);
    action->setProperty("componentName", component());
    action->setProperty("componentDisplayName", QGuiApplication::applicationDisplayName());
    connect(action, &QAction::triggered, this, [this, id] { Q_EMIT activated(id); });
    m_actions.insert(id, action);
    KGlobalAccel::setGlobalShortcut(action, key);
}

QKeySequence KdeGlobalShortcuts::key(const QString &id) const
{
    const QAction *action = m_actions.value(id);
    return action ? KGlobalAccel::self()->shortcut(action).value(0) : QKeySequence();
}

bool KdeGlobalShortcuts::setKey(const QString &id, const QKeySequence &key)
{
    QAction *action = m_actions.value(id);
    if (!action)
        return false;
    if (key == this->key(id))
        return true;
    // KGlobalAccel would take one that's in use and leave it with no key at
    // all. The service counts our other shortcuts in this as well.
    if (!key.isEmpty() && !KGlobalAccel::isGlobalShortcutAvailable(key, component()))
        return false;
    KGlobalAccel::self()->setShortcut(action, {key}, KGlobalAccel::NoAutoloading);
    return this->key(id) == key;
}

} // namespace pou
