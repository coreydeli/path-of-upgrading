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
                const QString id = action->objectName();
                if (m_actions.value(id) != action)
                    return;
                m_keys.insert(id, key);
                Q_EMIT keyChanged(id, key);
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
    Q_ASSERT(!m_actions.contains(id));
    auto *action = new QAction(name, this);
    action->setObjectName(id);
    action->setProperty("componentName", component());
    action->setProperty("componentDisplayName", QGuiApplication::applicationDisplayName());
    connect(action, &QAction::triggered, this, [this, id] { Q_EMIT activated(id); });
    m_actions.insert(id, action);
    m_keys.insert(id, key);
    if (m_grabbed)
        grab(id);
}

bool KdeGlobalShortcuts::grab(const QString &id)
{
    QAction *action = m_actions.value(id);
    const QKeySequence wanted = m_keys.value(id);
    KGlobalAccel::setGlobalShortcut(action, wanted);
    // the desktop may have had another, or someone else may have this one
    const QKeySequence given = KGlobalAccel::self()->shortcut(action).value(0);
    m_keys.insert(id, given);
    return given != wanted;
}

void KdeGlobalShortcuts::setGrabbed(bool grabbed)
{
    if (grabbed == m_grabbed)
        return;
    const QStringList ids = m_actions.keys();
    // what the desktop has, which it's about to forget
    if (!grabbed) {
        for (const QString &id : ids)
            m_keys.insert(id, key(id));
    }
    m_grabbed = grabbed;
    for (const QString &id : ids) {
        if (!grabbed)
            // which forgets the key too, so the next start has the app's
            KGlobalAccel::self()->removeAllShortcuts(m_actions.value(id));
        else if (grab(id))
            Q_EMIT keyChanged(id, m_keys.value(id));
    }
}

QKeySequence KdeGlobalShortcuts::key(const QString &id) const
{
    const QAction *action = m_actions.value(id);
    if (!action)
        return QKeySequence();
    return m_grabbed ? KGlobalAccel::self()->shortcut(action).value(0) : m_keys.value(id);
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
    if (!m_grabbed) {
        m_keys.insert(id, key);
        return true;
    }
    KGlobalAccel::self()->setShortcut(action, {key}, KGlobalAccel::NoAutoloading);
    m_keys.insert(id, this->key(id));
    return this->key(id) == key;
}

} // namespace pou
