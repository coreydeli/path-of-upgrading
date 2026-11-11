// SPDX-License-Identifier: MIT
#pragma once

#include <QString>

namespace pou {

// Where the project lives online, for the About and Help pages.
QString repositoryUrl();
QString releasesUrl();
QString issuesUrl();
QString wikiUrl();
// A page of the wiki by its name, as it's in the page's address ("Hotkeys-and-the-Overlay").
QString wikiPageUrl(const QString &page);

} // namespace pou
