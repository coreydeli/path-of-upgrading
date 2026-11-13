// SPDX-License-Identifier: MIT
#include "platform/kde/kdesystemclipboard.h"

#include <KSystemClipboard>
#include <QMimeData>

namespace pou {

KdeSystemClipboard::KdeSystemClipboard(QObject *parent)
    : SystemClipboard(parent)
    , m_clipboard(KSystemClipboard::instance())
{
    if (!m_clipboard)
        return;
    connect(m_clipboard, &KSystemClipboard::changed, this, [this](QClipboard::Mode mode) {
        // not the selection, which is whatever text is highlighted
        if (mode == QClipboard::Clipboard)
            Q_EMIT changed();
    });
}

std::unique_ptr<QMimeData> KdeSystemClipboard::contents()
{
    return m_clipboard ? copyOf(m_clipboard->mimeData(QClipboard::Clipboard)) : nullptr;
}

QString KdeSystemClipboard::text()
{
    return m_clipboard ? m_clipboard->text(QClipboard::Clipboard) : QString();
}

void KdeSystemClipboard::setContents(std::unique_ptr<QMimeData> data)
{
    // which takes it
    if (m_clipboard)
        m_clipboard->setMimeData(data.release(), QClipboard::Clipboard);
}

} // namespace pou
