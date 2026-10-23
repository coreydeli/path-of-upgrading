// SPDX-License-Identifier: MIT
#pragma once

#include "platform/globalshortcuts.h"

#include <QHash>

class QAction;

namespace pou {

// Through KDE's global shortcuts service, which KWin runs on Plasma.
class KdeGlobalShortcuts : public GlobalShortcuts
{
    Q_OBJECT

public:
    explicit KdeGlobalShortcuts(QObject *parent = nullptr);

    bool isAvailable() const override;
    QString errorString() const override;
    void add(const QString &id, const QString &name, const QKeySequence &key) override;
    QKeySequence key(const QString &id) const override;
    bool setKey(const QString &id, const QKeySequence &key) override;

private:
    QHash<QString, QAction *> m_actions;
};

} // namespace pou
