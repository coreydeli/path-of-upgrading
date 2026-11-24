// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace pou {

// The looks the app has. Dossier is the app's own and the default, Material
// follows Google's Material Design 3, and Slate is a dense, calm panel,
// Pocket is rounded and roomy,
// and Edge is flat with a colored edge for the meaning,
// and Gilt is quiet and premium, with thin gold lines,
// and Cinder is square and bold, with hard shadows and monospace labels.
enum class Design { Dossier, Material, Slate, Pocket, Edge, Gilt, Cinder };

// What the app knows of a look outside its drawing: what the pickers say about
// it, the faces its headings and text are set in, and its color schemes. The
// drawing is the QML Look's table, under the same id.
struct DesignInfo
{
    Design design;
    QString id;
    QString name;
    // one line for the pickers
    QString note;
    // a font family, or empty for the desktop's
    QString headingFont;
    QString textFont;
    // The color scheme files are named after it, with Light or Dark. Empty
    // when the look has the one dark scheme.
    QString schemeName;
    bool hasLight;
    // whether Kirigami's own corners are round
    bool roundCorners;
    // how the gear score and the verdict are drawn: diamonds, strip, ring,
    // edge, hairline or segments
    QString tile;
    // background, panel, accent and text of the dark palette, for the pickers
    QStringList swatch;
};

// Every look, in the order the pickers list them.
const QList<DesignInfo> &designs();
const DesignInfo &designInfo(Design design);

QString designId(Design design);
// Dossier for an id it doesn't know
Design designFromId(QStringView id);

} // namespace pou
