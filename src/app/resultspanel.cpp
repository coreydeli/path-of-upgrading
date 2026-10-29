// SPDX-License-Identifier: MIT
#include "app/resultspanel.h"

#include "app/traderesults.h"
#include "core/placement.h"
#include "platform/gameinput.h"
#include "platform/overlay.h"

#include <QQmlComponent>
#include <QQuickWindow>
#include <QScreen>

using namespace Qt::Literals::StringLiterals;

namespace pou {

ResultsPanel::ResultsPanel(QQmlEngine *engine, GameInput *input, TradeResults *results, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_input(input)
    , m_results(results)
{
    // a new search is what you want to see, with it open
    connect(results, &TradeResults::searchChanged, this, [this] {
        if (isVisible())
            m_results->ask();
    });
}

ResultsPanel::~ResultsPanel() = default;

void ResultsPanel::setSettings(const OverlaySettings &settings)
{
    m_settings = settings;
    Q_EMIT settingsChanged();
    // as the popup may have moved to the other side
    if (isVisible())
        place();
}

void ResultsPanel::setUnavailable(const QString &why)
{
    if (why == m_unavailable)
        return;
    m_unavailable = why;
    Q_EMIT unavailableChanged();
}

void ResultsPanel::show(const QString &screen)
{
    // once it's open it stays on its screen, unless it's told another
    if (!screen.isEmpty()) {
        m_screen = screen;
    } else if (!isVisible()) {
        if (const std::optional<ScreenPoint> mouse = m_input->mouse())
            m_screen = mouse->screen;
    }
    const QScreen *onScreen = screenNamed(m_screen);
    if (!onScreen)
        return;
    const int maxHeight = maxResultsHeight(onScreen->size());
    if (maxHeight != m_maxHeight) {
        m_maxHeight = maxHeight;
        Q_EMIT maxHeightChanged();
    }

    if (!m_window && !makeWindow())
        return;
    m_results->ask();
    place();
    m_window->show();
}

void ResultsPanel::toggle()
{
    if (isVisible())
        close();
    else
        show();
}

void ResultsPanel::close()
{
    if (m_window)
        m_window->hide();
}

bool ResultsPanel::isVisible() const
{
    return m_window && m_window->isVisible();
}

bool ResultsPanel::makeWindow()
{
    QQmlComponent component(m_engine, "PathOfUpgrading", "ResultsWindow");
    std::unique_ptr<QObject> made(
        component.createWithInitialProperties({{u"panel"_s, QVariant::fromValue(this)}}));
    auto *window = qobject_cast<QQuickWindow *>(made.get());
    if (!window) {
        qWarning("Couldn't make the results window: %s", qPrintable(component.errorString()));
        return false;
    }
    made.release();
    m_window.reset(window);
    makeOverlay(window);
    return true;
}

void ResultsPanel::place()
{
    QScreen *screen = screenNamed(m_screen);
    if (!screen)
        return;
    moveOverlay(m_window.get(), screen, placeResults(m_settings.position, m_window->size(), screen->size()));
}

} // namespace pou
