// SPDX-License-Identifier: MIT
#include "app/fonts.h"

#include <QFontDatabase>

using namespace Qt::Literals::StringLiterals;

namespace pou {

QString headingFontFamily()
{
    return u"Cinzel"_s;
}

QStringList scalableFamilies()
{
    QStringList families = QFontDatabase::families();
    families.removeIf([](const QString &family) { return !QFontDatabase::isSmoothlyScalable(family); });
    return families;
}

bool addBundledFonts()
{
    // a variable font, so the one file has every weight
    return QFontDatabase::addApplicationFont(u":/fonts/Cinzel.ttf"_s) != -1;
}

} // namespace pou
