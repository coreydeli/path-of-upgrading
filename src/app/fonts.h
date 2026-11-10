// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>

namespace pou {

// The heading font the app carries, and the one for text.
QString headingFontFamily();
QString textFontFamily();

// The installed font families that scale to any size. Bitmap fonts are left
// out, as they only look right at the sizes they were made for.
QStringList scalableFamilies();

// Adds the fonts in the program's resources to the font database. False when
// a font can't be read.
bool addBundledFonts();

} // namespace pou
