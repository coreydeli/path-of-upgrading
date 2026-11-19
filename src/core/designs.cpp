// SPDX-License-Identifier: MIT
#include "core/designs.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

const QList<DesignInfo> &designs()
{
    static const QList<DesignInfo> all = {
        {.design = Design::Dossier,
         .id = u"dossier"_s,
         .name = u"Dossier"_s,
         .note = u"The app's own look, dark with cut corners"_s,
         .headingFont = u"Big Shoulders Display"_s,
         .textFont = u"Spectral"_s,
         .schemeName = {},
         .hasLight = false,
         .roundCorners = false,
         .swatch = {u"#0c0c0d"_s, u"#161618"_s, u"#35b5a8"_s, u"#ece6da"_s}},
        {.design = Design::Material,
         .id = u"material"_s,
         .name = u"Material"_s,
         .note = u"Google's Material Design 3, light or dark"_s,
         .headingFont = {},
         .textFont = {},
         .schemeName = u"Material"_s,
         .hasLight = true,
         .roundCorners = true,
         .swatch = {u"#141218"_s, u"#211f26"_s, u"#d0bcff"_s, u"#e6e0e9"_s}},
    };
    return all;
}

const DesignInfo &designInfo(Design design)
{
    for (const DesignInfo &info : designs()) {
        if (info.design == design)
            return info;
    }
    return designs().first();
}

QString designId(Design design)
{
    return designInfo(design).id;
}

Design designFromId(QStringView id)
{
    for (const DesignInfo &info : designs()) {
        if (info.id == id)
            return info.design;
    }
    return Design::Dossier;
}

} // namespace pou
