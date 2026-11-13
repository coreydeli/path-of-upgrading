// SPDX-License-Identifier: MIT
#pragma once

#include "platform/systemclipboard.h"

#include <QMimeData>

// The clipboard for tests, which never touches the desktop's. A copy is
// whatever the test says was copied.
class FakeClipboard : public pou::SystemClipboard
{
public:
    using pou::SystemClipboard::SystemClipboard;

    std::unique_ptr<QMimeData> contents() override
    {
        ++reads;
        return pou::copyOf(m_data.get());
    }

    QString text() override { return m_data ? m_data->text() : QString(); }

    void setContents(std::unique_ptr<QMimeData> data) override
    {
        ++sets;
        m_data = std::move(data);
        Q_EMIT changed();
    }

    // As another program would, without counting it as set.
    void copy(const QString &text, bool tell = true)
    {
        m_data = std::make_unique<QMimeData>();
        m_data->setText(text);
        if (tell)
            Q_EMIT changed();
    }

    // What's on it in this format.
    QByteArray data(const QString &format) const { return m_data ? m_data->data(format) : QByteArray(); }
    void setData(const QString &format, const QByteArray &bytes)
    {
        if (!m_data)
            m_data = std::make_unique<QMimeData>();
        m_data->setData(format, bytes);
    }

    int reads = 0;
    int sets = 0;

private:
    std::unique_ptr<QMimeData> m_data;
};
