// SPDX-License-Identifier: MIT
#include "app/about.h"

#include "app/poboptions.h"
#include "core/links.h"
#include "core/settings.h"
#include "platform/opener.h"

#include <KColorScheme>
#include <QCoreApplication>
#include <QUrl>
#include <QVariantMap>
#include <kcolorscheme_version.h>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

struct Credit
{
    QString name;
    QString what;
    QString license;
    QString url;
};

QList<Credit> creditList()
{
    return {
        {u"Path of Building Community"_s,
         u"Does the calculations. It isn't included or changed, and the copy you have runs without its window."_s,
         u"MIT"_s, u"https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2"_s},
        {u"rusty-path-of-building"_s,
         u"Makes Path of Building run on Linux, and is its Flatpak on Flathub."_s, u"MIT"_s,
         u"https://github.com/meehl/rusty-path-of-building"_s},
        {u"Cinzel"_s, u"The font of the headings, by The Cinzel Project Authors."_s,
         u"SIL Open Font License 1.1"_s, u"https://github.com/NDISCOVER/Cinzel"_s},
        {u"poe.ninja"_s, u"Where the currency prices come from."_s, QString(), u"https://poe.ninja"_s},
        {u"Path of Exile trade site"_s, u"Where the listings come from."_s, QString(),
         u"https://www.pathofexile.com/trade2"_s},
    };
}

} // namespace

About::About(PobOptions *pob, Opener *opener, QObject *parent)
    : QObject(parent)
    , m_pob(pob)
    , m_opener(opener)
{
    connect(pob, &PobOptions::changed, this, &About::pobStatusChanged);
}

QString About::version() const
{
    const QString version = QCoreApplication::applicationVersion();
    return version.isEmpty() ? u"unknown"_s : version;
}

QString About::edition() const
{
    return isBetaBuild() ? u"Beta"_s : u"Release"_s;
}

QString About::license() const
{
    return u"MIT"_s;
}

QString About::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString About::frameworksVersion() const
{
    // the colors library is versioned with the rest of KDE Frameworks
    return QStringLiteral(KCOLORSCHEME_VERSION_STRING);
}

QString About::pobStatus() const
{
    const QString at = m_pob->installedAt();
    return at.isEmpty() ? u"Not found"_s : at;
}

QVariantList About::credits() const
{
    QVariantList list;
    for (const Credit &credit : creditList())
        list << QVariantMap{{u"name"_s, credit.name},
                            {u"what"_s, credit.what},
                            {u"license"_s, credit.license},
                            {u"url"_s, credit.url}};
    return list;
}

void About::openRepository()
{
    open(repositoryUrl());
}

void About::openReleases()
{
    open(releasesUrl());
}

void About::openIssues()
{
    open(issuesUrl());
}

void About::openCredit(int index)
{
    const QList<Credit> list = creditList();
    if (index >= 0 && index < list.size())
        open(list.at(index).url);
}

void About::open(const QString &url)
{
    if (m_opener)
        m_opener->open(QUrl(url));
}

} // namespace pou
