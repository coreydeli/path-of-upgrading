// SPDX-License-Identifier: MIT
#include "platform/overlay.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#ifndef Q_OS_WIN
#include <LayerShellQt/Window>
#endif

namespace pou {
namespace {

#ifndef Q_OS_WIN
using Layer = LayerShellQt::Window;
#endif

bool isWayland()
{
    return QGuiApplication::platformName().startsWith(u"wayland");
}

} // namespace

QScreen *screenNamed(const QString &name)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen->name() == name)
            return screen;
    }
    return QGuiApplication::primaryScreen();
}

void makeOverlay(QWindow *window)
{
    window->setFlags(window->flags() | Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
#ifdef Q_OS_WIN
    window->setFlags(window->flags() | Qt::WindowStaysOnTopHint);
#else
    if (!isWayland()) {
        // without the window manager, the X server stacks it over everything
        // the window manager has, a fullscreen game too
        window->setFlags(window->flags() | Qt::BypassWindowManagerHint);
        return;
    }

    Layer *layer = Layer::get(window);
    layer->setLayer(Layer::LayerOverlay);
    layer->setScope(QCoreApplication::applicationName());
    layer->setKeyboardInteractivity(Layer::KeyboardInteractivityNone);
    // placed from the screen's corner, and not from where its panels end
    layer->setAnchors({Layer::AnchorTop, Layer::AnchorLeft});
    layer->setExclusiveZone(-1);
#endif
}

void moveOverlay(QWindow *window, QScreen *screen, const QPoint &position)
{
    if (!isWayland()) {
        window->setPosition(screen->geometry().topLeft() + position);
        return;
    }

#ifndef Q_OS_WIN
    // the compositor keeps a layer surface on the screen it was made for
    const bool remake = window->isVisible() && window->screen() != screen;
    if (remake)
        window->hide();
    window->setScreen(screen);
    Layer::get(window)->setMargins({position.x(), position.y(), 0, 0});
    if (remake)
        window->show();
    else if (window->isVisible())
        // the margins only count at the surface's next commit, which a window
        // with nothing new to draw doesn't make
        window->requestUpdate();
#endif
}

} // namespace pou
