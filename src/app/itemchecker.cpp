// SPDX-License-Identifier: MIT
#include "app/itemchecker.h"

#include "app/itemreport.h"

namespace pou {

ItemChecker::ItemChecker(QQmlEngine *engine, GameInput *input, QObject *parent)
    : QObject(parent)
    , m_reader(input)
    , m_popup(engine, input)
{
    connect(&m_reader, &ItemReader::itemRead, this, &ItemChecker::read);
    connect(&m_reader, &ItemReader::failed, &m_popup, &ItemPopup::showMessage);
    connect(&m_judge, &ItemJudge::checked, this, &ItemChecker::judged);
}

void ItemChecker::setSettings(const Settings &settings)
{
    const OverlaySettings &overlay = settings.overlay;
    m_reader.setHotkey(overlay.hotkey);
    m_reader.setAdvancedCopy(overlay.advancedCopy);
    m_reader.setWatchClipboard(overlay.watchClipboard);
    m_reader.setPaused(overlay.paused);
    m_popup.setSettings(overlay);
}

void ItemChecker::setCharacter(const Character &character)
{
    m_judge.setCharacter(character);
}

void ItemChecker::check()
{
    m_reader.read();
}

void ItemChecker::read(const Item &item, const std::optional<ScreenPoint> &mouse)
{
    Q_EMIT itemRead(item);
    m_mouse = mouse;
    // which says how it does before it returns
    m_judge.check(item);
}

void ItemChecker::judged(const ItemCheck &check)
{
    m_popup.showReport(reportOf(check), m_mouse);
}

} // namespace pou
