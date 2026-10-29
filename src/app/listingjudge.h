// SPDX-License-Identifier: MIT
#pragma once

#include "app/itemjudge.h"
#include "core/settings.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <optional>

namespace pou {

// Judges the listings of a trade search for the character you play, the way
// ItemJudge judges the item you check. Path of Building is asked about a few
// at a time, so an item you check meanwhile doesn't wait long behind them.
class ListingJudge : public QObject
{
    Q_OBJECT

public:
    explicit ListingJudge(QObject *parent = nullptr);
    ~ListingJudge() override;

    // Each of these judges them all again, as once you wear an item.
    void setCharacter(const Character &character, const std::optional<PobBuild> &build);
    void setPob(PobEngine *pob);
    void setSettings(const PobSettings &settings);

    // Judges these after the ones it has.
    void add(const QList<Item> &items);
    void clear();
    // In the order they were added.
    const QList<ItemCheck> &checks() const { return m_checks; }

Q_SIGNALS:
    // When those from first to last in checks() have changed, which isn't
    // when they're added.
    void checked(qsizetype first, qsizetype last);

private:
    void judgeAll();
    void askPob();
    void answered();

    PobEngine *m_pob = nullptr;
    PobSettings m_settings;
    Judging m_judging;
    QList<ItemCheck> m_checks;
    QPointer<PobReply> m_reply;
    QList<qsizetype> m_asked; // what it's for, in checks()
};

} // namespace pou
