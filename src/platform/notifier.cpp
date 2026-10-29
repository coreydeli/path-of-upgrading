// SPDX-License-Identifier: MIT
#include "platform/notifier.h"

#include "platform/kde/kdenotifier.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

QString eventId(Notifier::Event event)
{
    switch (event) {
    case Notifier::Event::HotkeysUnavailable:
        return u"hotkeysUnavailable"_s;
    }
    Q_UNREACHABLE_RETURN({});
}

std::unique_ptr<Notifier> createNotifier()
{
    return std::make_unique<KdeNotifier>();
}

} // namespace pou
