// SPDX-License-Identifier: MIT
#include "platform/systemclipboard.h"

#ifdef Q_OS_WIN
#include "platform/qtsystemclipboard.h"
#else
#include "platform/kde/kdesystemclipboard.h"
#endif

#include <QMimeData>

namespace pou {

std::unique_ptr<SystemClipboard> createSystemClipboard()
{
#ifdef Q_OS_WIN
    return std::make_unique<QtSystemClipboard>();
#else
    return std::make_unique<KdeSystemClipboard>();
#endif
}

std::unique_ptr<QMimeData> copyOf(const QMimeData *data, qsizetype mostBytes, QDeadlineTimer deadline)
{
    if (!data)
        return nullptr;
    auto copy = std::make_unique<QMimeData>();
    qsizetype size = 0;
    for (const QString &format : data->formats()) {
        // which asks the program that copied it, on Wayland and X
        const QByteArray bytes = data->data(format);
        size += bytes.size();
        if (size > mostBytes || deadline.hasExpired())
            return nullptr;
        if (!bytes.isEmpty())
            copy->setData(format, bytes);
    }
    if (copy->formats().isEmpty())
        return nullptr;
    return copy;
}

} // namespace pou
