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
         .tile = u"diamonds"_s,
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
         .tile = u"diamonds"_s,
         .swatch = {u"#141218"_s, u"#211f26"_s, u"#d0bcff"_s, u"#e6e0e9"_s}},
        {.design = Design::Slate,
         .id = u"slate"_s,
         .name = u"Slate"_s,
         .note = u"A dense, calm panel for reading a lot of mods quickly"_s,
         .headingFont = u"Alegreya SC"_s,
         .textFont = u"Inter"_s,
         .schemeName = u"Slate"_s,
         .hasLight = true,
         .roundCorners = false,
         .tile = u"strip"_s,
         .swatch = {u"#1b2230"_s, u"#243044"_s, u"#e0b45a"_s, u"#dfe6f2"_s}},
        {.design = Design::Pocket,
         .id = u"pocket"_s,
         .name = u"Pocket"_s,
         .note = u"Rounded, roomy cards with one loud color"_s,
         .headingFont = u"Inter"_s,
         .textFont = u"Inter"_s,
         .schemeName = u"Pocket"_s,
         .hasLight = true,
         .roundCorners = true,
         .tile = u"ring"_s,
         .swatch = {u"#0b0b0d"_s, u"#161618"_s, u"#4aa3ff"_s, u"#f2f2f4"_s}},
        {.design = Design::Edge,
         .id = u"edge"_s,
         .name = u"Edge"_s,
         .note = u"Flat and high contrast, with a colored edge that says it at a glance"_s,
         .headingFont = u"Barlow Semi Condensed"_s,
         .textFont = u"IBM Plex Sans"_s,
         .schemeName = u"Edge"_s,
         .hasLight = true,
         .roundCorners = false,
         .tile = u"edge"_s,
         .swatch = {u"#06080a"_s, u"#10161c"_s, u"#2dd4a8"_s, u"#e5e8eb"_s}},
        {.design = Design::Gilt,
         .id = u"gilt"_s,
         .name = u"Gilt"_s,
         .note = u"Quiet and premium, with thin gold lines and serif titles"_s,
         .headingFont = u"Cinzel"_s,
         .textFont = u"Inter"_s,
         .schemeName = u"Gilt"_s,
         .hasLight = true,
         .roundCorners = false,
         .tile = u"hairline"_s,
         .swatch = {u"#0d0c0f"_s, u"#16141a"_s, u"#d4a95a"_s, u"#ece6d8"_s}},
        {.design = Design::Cinder,
         .id = u"cinder"_s,
         .name = u"Cinder"_s,
         .note = u"Square and bold, with hard shadows and monospace labels"_s,
         .headingFont = u"Space Grotesk"_s,
         .textFont = u"Space Grotesk"_s,
         .schemeName = u"Cinder"_s,
         .hasLight = true,
         .roundCorners = false,
         .tile = u"segments"_s,
         .swatch = {u"#120f0d"_s, u"#1c1613"_s, u"#f0762c"_s, u"#f1e7dd"_s}},
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
