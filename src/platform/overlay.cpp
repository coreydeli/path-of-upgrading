// SPDX-License-Identifier: MIT
#include "platform/overlay.h"

#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

namespace pou {
namespace {

using Layer = LayerShellQt::Window;

bool isWayland()
{
    return QGuiApplication::platformName().startsWith(u"wayland");
}

} // namespace

void makeOverlay(QWindow *window)
{
    window->setFlags(window->flags() | Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
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
}

void moveOverlay(QWindow *window, QScreen *screen, const QPoint &position)
{
    if (!isWayland()) {
        window->setPosition(screen->geometry().topLeft() + position);
        return;
    }

    // the compositor keeps a layer surface on the screen it was made for
    const bool remake = window->isVisible() && window->screen() != screen;
    if (remake)
        window->hide();
    window->setScreen(screen);
    Layer::get(window)->setMargins({position.x(), position.y(), 0, 0});
    if (remake)
        window->show();
}

} // namespace pou
