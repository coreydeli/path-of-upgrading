// SPDX-License-Identifier: MIT
#pragma once

#include <QString>

namespace pou {

// The heading font the app carries.
QString headingFontFamily();

// Adds the fonts in the program's resources to the font database. False when
// a font can't be read.
bool addBundledFonts();

} // namespace pou
