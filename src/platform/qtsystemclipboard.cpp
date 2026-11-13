// SPDX-License-Identifier: MIT
#include "platform/qtsystemclipboard.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

namespace pou {

QtSystemClipboard::QtSystemClipboard(QObject *parent)
    : SystemClipboard(parent)
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &SystemClipboard::changed);
}

std::unique_ptr<QMimeData> QtSystemClipboard::contents()
{
    return copyOf(QGuiApplication::clipboard()->mimeData());
}

QString QtSystemClipboard::text()
{
    return QGuiApplication::clipboard()->text();
}

void QtSystemClipboard::setContents(std::unique_ptr<QMimeData> data)
{
    // which takes it
    QGuiApplication::clipboard()->setMimeData(data.release());
}

} // namespace pou
