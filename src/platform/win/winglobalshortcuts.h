// SPDX-License-Identifier: MIT
#pragma once

#include "platform/globalshortcuts.h"

#include <QAbstractNativeEventFilter>
#include <QHash>

namespace pou {

// With RegisterHotKey. Windows doesn't keep them between runs, so they start
// with the keys the settings have.
class WinGlobalShortcuts : public GlobalShortcuts, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WinGlobalShortcuts(QObject *parent = nullptr);
    ~WinGlobalShortcuts() override;

    bool isAvailable() const override { return true; }
    QString errorString() const override { return {}; }
    void add(const QString &id, const QString &name, const QKeySequence &key) override;
    QKeySequence key(const QString &id) const override;
    bool setKey(const QString &id, const QKeySequence &key) override;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    struct Shortcut
    {
        int number = 0;   // what Windows knows it by
        QKeySequence key; // none when Windows wouldn't take it
    };

    bool take(int number, const QKeySequence &key);

    QHash<QString, Shortcut> m_shortcuts;
    int m_lastNumber = 0;
};

} // namespace pou
