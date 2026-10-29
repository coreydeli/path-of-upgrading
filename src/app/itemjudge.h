// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterstore.h"
#include "core/evaluation.h"
#include "core/item.h"
#include "core/pobbuild.h"
#include "core/pobverdict.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <optional>

namespace pou {

class PobEngine;
class PobReply;
struct PobItemEval;

// An item you checked and what it means for the character you play.
struct ItemCheck
{
    // Where Path of Building is with it.
    enum class Pob {
        Unused,  // it's off or not installed, or the item isn't one to try on
        NoBuild, // the character has no build imported to judge it with
        Checking,
        Checked, // and the evaluation is its judgement
        Failed,  // see pobError
    };

    Item item;
    Evaluation evaluation;
    Pob pob = Pob::Unused;
    QString pobError;
};

// What items are judged by.
struct Judging
{
    Character character;
    // The character's build, none when it has none. The engine should have it
    // with the gear the character wears, see pobWearing().
    std::optional<PobBuild> build;
    // Path of Building, or none to judge items without it.
    PobEngine *engine = nullptr;
    // What matters to you, for the gear scores too. Chaos Inoculation goes
    // by the character.
    PobVerdictOptions options;
};

// How the item does by its gear scores or your weights. It's Checking when
// Path of Building is to be asked about it.
ItemCheck firstCheck(const Item &item, const Judging &judging);
// The check with what Path of Building said of the item, from the places it
// tried it in, or why it couldn't say.
ItemCheck withPobAnswer(ItemCheck check, const PobStats &before, const PobItemEval &tried,
                        const Judging &judging);

// Judges the items you check for the character you play, by its gear scores
// or your weights straight away, then by Path of Building once it answers.
class ItemJudge : public QObject
{
    Q_OBJECT

public:
    explicit ItemJudge(QObject *parent = nullptr);
    ~ItemJudge() override;

    void setCharacter(const Character &character);
    // The character's build, none when it has none. The engine should have it
    // with the gear the character wears, see pobWearing().
    void setBuild(const std::optional<PobBuild> &build);
    // Path of Building, or none to judge items without it.
    void setEngine(PobEngine *engine);
    // What matters to you, for the gear scores too.
    void setPriorities(const PobPriorities &priorities);
    // How much of Path of Building's score is damage, from 0 to 1.
    void setDamageWeight(double weight);

    // Judges the item in place of the one before. checked() has how it does
    // straight away, and again once Path of Building has answered.
    void check(const Item &item);
    const ItemCheck &current() const { return m_check; }

Q_SIGNALS:
    void checked(const ItemCheck &check);

private:
    void answered();
    void fail(const QString &why);

    Judging m_judging;
    ItemCheck m_check;
    QPointer<PobReply> m_reply; // for the item being checked
};

} // namespace pou
