// SPDX-License-Identifier: MIT
#include "app/fonts.h"

#include <QFontDatabase>

using namespace Qt::Literals::StringLiterals;

namespace pou {

QString headingFontFamily()
{
    return u"Big Shoulders Display"_s;
}

QString textFontFamily()
{
    return u"Spectral"_s;
}

QStringList scalableFamilies()
{
    QStringList families = QFontDatabase::families();
    // some platforms list names, like "Monospace", that setting a font by name won't take
    families.removeIf([](const QString &family) {
        return !QFontDatabase::hasFamily(family) || !QFontDatabase::isSmoothlyScalable(family);
    });
    return families;
}

bool addBundledFonts()
{
    const QStringList files = {
        u"BigShouldersDisplay-Black"_s,
        u"BigShouldersDisplay-Bold"_s,
        u"BigShouldersDisplay-ExtraBold"_s,
        u"BigShouldersText-Bold"_s,
        u"BigShouldersText-ExtraBold"_s,
        u"BigShouldersText-Medium"_s,
        u"BigShouldersText-Regular"_s,
        u"BigShouldersText-SemiBold"_s,
        u"Spectral-Bold"_s,
        u"Spectral-Italic"_s,
        u"Spectral-Medium"_s,
        u"Spectral-Regular"_s,
        u"Spectral-SemiBold"_s,
    };
    bool all = true;
    for (const QString &file : files)
        all = QFontDatabase::addApplicationFont(u":/fonts/"_s + file + u".ttf"_s) != -1 && all;
    return all;
}

} // namespace pou
