// SPDX-License-Identifier: MIT
#include "core/links.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

QString repositoryUrl()
{
    return u"https://github.com/coreydeli/path-of-upgrading"_s;
}

QString releasesUrl()
{
    return repositoryUrl() + u"/releases"_s;
}

QString issuesUrl()
{
    return repositoryUrl() + u"/issues"_s;
}

QString wikiUrl()
{
    return repositoryUrl() + u"/wiki"_s;
}

QString wikiPageUrl(const QString &page)
{
    return wikiUrl() + u'/' + page;
}

} // namespace pou
