// SPDX-License-Identifier: MIT
#include "platform/kde/kdetray.h"

#include <KStatusNotifierItem>
#include <QAction>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Named after the app's desktop file, as the icon is.
QString appId()
{
    const QString desktopFile = QGuiApplication::desktopFileName();
    return desktopFile.isEmpty() ? QCoreApplication::applicationName() : desktopFile;
}

} // namespace

KdeTray::KdeTray(QObject *parent)
    : Tray(parent)
    , m_item(new KStatusNotifierItem(appId(), this))
{
    m_item->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_item->setTitle(QGuiApplication::applicationDisplayName());
    m_item->setIconByName(appId());
    m_item->setToolTip(appId(), QGuiApplication::applicationDisplayName(), QString());
    // Passive would let the tray tuck it away
    m_item->setStatus(KStatusNotifierItem::Active);

    // which would add its own Restore next to Show, and Quit
    m_item->setStandardActionsEnabled(false);
    auto *menu = new QMenu;
    QAction *show = menu->addAction(u"Show Path of Upgrading"_s);
    connect(show, &QAction::triggered, this, &Tray::showRequested);
    m_pause = menu->addAction(u"Pause the Hotkeys"_s);
    m_pause->setCheckable(true);
    connect(m_pause, &QAction::triggered, this, &Tray::pauseRequested);
    menu->addSeparator();
    QAction *quit = menu->addAction(QIcon::fromTheme(u"application-exit"_s), u"Quit"_s);
    connect(quit, &QAction::triggered, qApp, &QCoreApplication::quit);
    // which the item keeps
    m_item->setContextMenu(menu);
}

void KdeTray::setWindow(QWindow *window)
{
    m_item->setAssociatedWindow(window);
}

void KdeTray::setPaused(bool paused)
{
    m_pause->setChecked(paused);
    m_item->setToolTipSubTitle(paused ? u"The hotkeys are paused."_s : QString());
}

} // namespace pou
