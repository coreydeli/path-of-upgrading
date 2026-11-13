// SPDX-License-Identifier: MIT
#include "core/pobinstall.h"

#include <QXmlStreamReader>

using namespace Qt::Literals::StringLiterals;

namespace pou {

QString chosenBuildsFolder(const QByteArray &settings)
{
    QXmlStreamReader reader(settings);
    if (!reader.readNextStartElement())
        return {};
    while (reader.readNextStartElement()) {
        if (reader.name() == u"Misc")
            return reader.attributes().value(u"buildPath").toString();
        reader.skipCurrentElement();
    }
    return {};
}

} // namespace pou
