// SPDX-License-Identifier: MIT
#include "app/itemchecker.h"

#include "app/itemreport.h"
#include "app/tradereport.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

ItemChecker::ItemChecker(QQmlEngine *engine, GameInput *input, QObject *parent)
    : QObject(parent)
    , m_input(input)
    , m_reader(input)
    , m_popup(engine, input)
{
    // until it's copied, or hasn't
    connect(&m_reader, &ItemReader::slow, this,
            [this] { m_popup.showMessage(u"The game is still copying the item."_s, m_input->mouse()); });
    connect(&m_reader, &ItemReader::itemRead, this, &ItemChecker::read);
    connect(&m_reader, &ItemReader::failed, &m_popup, &ItemPopup::showMessage);
    connect(&m_judge, &ItemJudge::checked, this, &ItemChecker::judged);
    connect(&m_popup, &ItemPopup::tradeAsked, this, &ItemChecker::findOnTrade);
}

void ItemChecker::setSettings(const Settings &settings)
{
    const OverlaySettings &overlay = settings.overlay;
    m_reader.setHotkey(overlay.hotkey);
    m_reader.setAdvancedCopy(overlay.advancedCopy);
    m_reader.setWatchClipboard(overlay.watchClipboard);
    m_reader.setPaused(overlay.paused);
    m_popup.setSettings(overlay);

    m_judge.setPriorities(settings.pob.priorities);
    m_judge.setDamageWeight(settings.pob.damageWeight);
    m_pobOn = settings.pob.enabled;
    judgeWithPob();
}

void ItemChecker::setCharacter(const Character &character, const std::optional<PobBuild> &build)
{
    m_judge.setCharacter(character);
    m_judge.setBuild(build);
}

void ItemChecker::setPob(PobEngine *pob)
{
    m_pob = pob;
    judgeWithPob();
}

void ItemChecker::setFinder(TradeFinder *finder)
{
    m_finder = finder;
    connect(finder, &TradeFinder::finished, this, &ItemChecker::found);
    connect(finder, &TradeFinder::waiting, this,
            [this](std::chrono::milliseconds wait) { m_popup.setTrade(searchingReport(wait)); });
}

void ItemChecker::check()
{
    m_reader.read();
}

void ItemChecker::read(const Item &item, const std::optional<ScreenPoint> &mouse)
{
    Q_EMIT itemRead(item);
    // what was asked for the one before is no use now
    if (m_finder)
        m_finder->cancel();
    m_mouse = mouse;
    m_fresh = true;
    // which says how it does before it returns
    m_judge.check(item);
}

void ItemChecker::judged(const ItemCheck &check)
{
    const ItemReport report = reportOf(check);
    if (m_fresh) {
        m_fresh = false;
        m_popup.showReport(report, m_mouse);
    } else {
        m_popup.updateReport(report);
    }
}

void ItemChecker::judgeWithPob()
{
    m_judge.setEngine(m_pobOn ? m_pob : nullptr);
}

void ItemChecker::findOnTrade(TradeFinder::Kind kind)
{
    if (!m_finder)
        return;
    m_popup.setTrade(searchingReport());
    m_finder->find(m_judge.current().item, kind);
}

void ItemChecker::found()
{
    const std::optional<TradeFinder::Found> &found = m_finder->found();
    m_popup.setTrade(found ? reportOf(*found) : failedReport(m_finder->errorString()));
}

} // namespace pou
