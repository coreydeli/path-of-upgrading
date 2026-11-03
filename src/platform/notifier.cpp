// SPDX-License-Identifier: MIT
#include "platform/notifier.h"

#ifdef Q_OS_WIN
#include "platform/traynotifier.h"
#else
#include "platform/kde/kdenotifier.h"
#endif

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

std::unique_ptr<Notifier> createNotifier([[maybe_unused]] Tray *tray)
{
#ifdef Q_OS_WIN
    return std::make_unique<TrayNotifier>(tray);
#else
    return std::make_unique<KdeNotifier>();
#endif
}

} // namespace pou
