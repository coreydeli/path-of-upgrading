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
    // some platforms list names, like "Monospace", that setting a font by name won't take
    families.removeIf([](const QString &family) {
        return !QFontDatabase::hasFamily(family) || !QFontDatabase::isSmoothlyScalable(family);
    });
    return families;
}

bool addBundledFonts()
{
    // a variable font, so the one file has every weight
    return QFontDatabase::addApplicationFont(u":/fonts/Cinzel.ttf"_s) != -1;
}

} // namespace pou
