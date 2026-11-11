// SPDX-License-Identifier: MIT
#include "app/help.h"

#include "core/links.h"
#include "platform/opener.h"

#include <QUrl>
#include <QVariantMap>

using namespace Qt::Literals::StringLiterals;

namespace pou {

Help::Help(Opener *opener, bool tradeOffered, QObject *parent)
    : QObject(parent)
    , m_opener(opener)
{
    m_topics = {
        {u"Getting Started"_s, u"Set it up and check your first item."_s, u"Getting-Started"_s},
        {u"The Popup"_s, u"What it says about an item, and what its buttons do."_s, u"The-Popup"_s},
        {u"Hotkeys and the Overlay"_s, u"The keys, and how the popup sits over the game."_s,
         u"Hotkeys-and-the-Overlay"_s},
        {u"Characters and Builds"_s, u"Adding characters and importing their builds."_s,
         u"Characters-and-Builds"_s},
        {u"Gear Score and Stat Weights"_s, u"How gear is rated, and how to change it."_s,
         u"Gear-Score-and-Stat-Weights"_s},
        {u"Path of Building"_s, u"Using your builds to judge items."_s, u"Path-of-Building"_s},
    };
    if (tradeOffered) {
        m_topics.append(
            {u"Trade Search"_s, u"Searching the trade site from the popup."_s, u"Trade-Search"_s});
        m_topics.append(
            {u"Trade Results"_s, u"The listings, judged for your character."_s, u"Trade-Results"_s});
    }
    m_topics.append({u"Appearance"_s, u"The colors and fonts."_s, u"Appearance"_s});
    m_topics.append(
        {u"Linux Desktops"_s, u"Wayland, X11 and the desktops it was tried on."_s, u"Linux-Desktops"_s});
    m_topics.append({u"Troubleshooting"_s, u"When something doesn't work."_s, u"Troubleshooting"_s});
    m_topics.append(
        {u"Your Data and Privacy"_s, u"What it keeps, and what it sends."_s, u"Your-Data-and-Privacy"_s});
}

QVariantList Help::topics() const
{
    QVariantList list;
    for (const Topic &topic : m_topics)
        list << QVariantMap{{u"title"_s, topic.title}, {u"what"_s, topic.what}, {u"page"_s, topic.page}};
    return list;
}

void Help::openWiki()
{
    open(wikiUrl());
}

void Help::openTopic(int index)
{
    if (index >= 0 && index < m_topics.size())
        open(wikiPageUrl(m_topics.at(index).page));
}

void Help::open(const QString &url)
{
    if (m_opener)
        m_opener->open(QUrl(url));
}

} // namespace pou
