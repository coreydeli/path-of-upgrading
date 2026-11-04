// SPDX-License-Identifier: MIT
#include "core/buildprofiles.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

ProfileStat stat(const QString &label, const QString &pattern, const PerSlot &importance, const PerSlot &cap,
                 const QString &key = {}, std::optional<Element> resist = {})
{
    return {label, QRegularExpression(pattern), importance, cap, key, resist};
}

// A stat that has exactly one key.
ProfileStat exact(const QString &label, const QString &key, const PerSlot &importance, const PerSlot &cap)
{
    return stat(label, u'^' + QRegularExpression::escape(key) + u'$', importance, cap, key);
}

ProfileStat resistance(Element e, double importance, double cap)
{
    ProfileStat s =
        exact(resistLabel(e), resistKey(e), PerSlot({{u"Weapon"_s, 0}, {u"Jewel"_s, 0}}, importance), cap);
    s.resist = e;
    return s;
}

ProfileStat baseEnergyShield()
{
    return stat(
        u"Energy Shield"_s, uR"(^\[Base\] Energy Shield$)"_s,
        PerSlot(
            {{u"Body Armour"_s, 10}, {u"Helmet"_s, 9}, {u"Gloves"_s, 8}, {u"Boots"_s, 7}, {u"Offhand"_s, 7}}),
        PerSlot({{u"Body Armour"_s, 1000},
                 {u"Helmet"_s, 550},
                 {u"Gloves"_s, 350},
                 {u"Boots"_s, 400},
                 {u"Offhand"_s, 350}},
                1));
}

// Flat Energy Shield on armour is local and already part of [Base] Energy
// Shield; on jewellery it's global.
ProfileStat flatEnergyShield(double factor = 1)
{
    return exact(u"Energy Shield"_s, u"# to maximum Energy Shield"_s,
                 PerSlot({{u"Amulet"_s, 6 * factor}, {u"Ring"_s, 5 * factor}, {u"Belt"_s, 4 * factor}}), 60);
}

ProfileStat percentEnergyShield(double factor = 1)
{
    return exact(u"% Energy Shield"_s, u"#% increased maximum Energy Shield"_s,
                 PerSlot({{u"Amulet"_s, 7 * factor}, {u"Jewel"_s, 7 * factor}}),
                 PerSlot({{u"Jewel"_s, 12}}, 25));
}

ProfileStat extraEnergyShield()
{
    return stat(u"Extra ES"_s, uR"(^Gain #% of maximum (Life|Mana) as Extra maximum Energy Shield$)"_s,
                PerSlot({{u"Body Armour"_s, 5}}), 15);
}

ProfileStat rarity()
{
    return exact(u"Rarity"_s, u"#% increased Rarity of Items found"_s,
                 PerSlot({{u"Ring"_s, 1.5},
                          {u"Amulet"_s, 1.5},
                          {u"Helmet"_s, 1.5},
                          {u"Gloves"_s, 1.5},
                          {u"Boots"_s, 1.5}}),
                 30);
}

ProfileStat movementSpeed()
{
    return exact(u"Movement speed"_s, u"#% increased Movement Speed"_s, PerSlot({{u"Boots"_s, 10}}), 35);
}

ProfileStat life(double importance)
{
    return exact(u"Life"_s, u"# to maximum Life"_s, PerSlot({{u"Weapon"_s, 0}, {u"Jewel"_s, 0}}, importance),
                 100);
}

ProfileStat mana(double importance = 3)
{
    return exact(u"Mana"_s, u"# to maximum Mana"_s, PerSlot({{u"Jewel"_s, 0}}, importance), 150);
}

ProfileStat weaponDps()
{
    return stat(u"Weapon DPS"_s, uR"(^\[Weapon\] Total DPS$)"_s, PerSlot({{u"Weapon"_s, 10}}), 400);
}

ProfileStat spirit(double importance)
{
    return exact(u"Spirit"_s, u"# to Spirit"_s,
                 PerSlot({{u"Body Armour"_s, importance}, {u"Amulet"_s, importance}}), 60);
}

// The utility stats below are worth a little to every build. Their slots and
// top rolls are the ones the game can roll (poe2db.tw, the mods of each item
// class), so a stat is only counted where it can come from.

// Flat on boots, body armour, belts and shields. Jewels roll a percentage.
ProfileStat stunThreshold()
{
    return exact(u"Stun threshold"_s, u"# to Stun Threshold"_s,
                 PerSlot({{u"Boots"_s, 1.5}, {u"Body Armour"_s, 1.5}, {u"Belt"_s, 1.5}, {u"Offhand"_s, 1.5}}),
                 PerSlot({{u"Boots"_s, 352}}, 304));
}

ProfileStat percentStunThreshold()
{
    return exact(u"Stun threshold"_s, u"#% increased Stun Threshold"_s, PerSlot({{u"Jewel"_s, 1}}), 16);
}

// On armour with Energy Shield, shields and foci. Jewels roll how soon it
// starts instead.
ProfileStat energyShieldRecharge(double importance)
{
    return exact(u"ES recharge"_s, u"#% increased Energy Shield Recharge Rate"_s,
                 PerSlot({{u"Helmet"_s, importance},
                          {u"Body Armour"_s, importance},
                          {u"Gloves"_s, importance},
                          {u"Boots"_s, importance},
                          {u"Offhand"_s, importance}}),
                 PerSlot({{u"Body Armour"_s, 27}, {u"Offhand"_s, 27}}, 19));
}

ProfileStat energyShieldRechargeStart(double importance)
{
    return exact(u"ES recharge"_s, u"#% faster start of Energy Shield Recharge"_s,
                 PerSlot({{u"Jewel"_s, importance}}), 15);
}

ProfileStat manaRegeneration(double importance)
{
    return exact(u"Mana regen"_s, u"#% increased Mana Regeneration Rate"_s,
                 PerSlot({{u"Ring"_s, importance},
                          {u"Amulet"_s, importance},
                          {u"Belt"_s, importance},
                          {u"Offhand"_s, importance},
                          {u"Jewel"_s, importance}}),
                 PerSlot({{u"Jewel"_s, 15}}, 69));
}

ProfileStat lifeRegeneration(double importance)
{
    return exact(u"Life regen"_s, u"# Life Regeneration per second"_s,
                 PerSlot({{u"Body Armour"_s, importance},
                          {u"Helmet"_s, importance},
                          {u"Boots"_s, importance},
                          {u"Belt"_s, importance},
                          {u"Amulet"_s, importance},
                          {u"Ring"_s, importance}}),
                 PerSlot({{u"Body Armour"_s, 36},
                          {u"Amulet"_s, 33},
                          {u"Belt"_s, 29},
                          {u"Helmet"_s, 23},
                          {u"Boots"_s, 23},
                          {u"Ring"_s, 18}}));
}

ProfileStat percentLifeRegeneration(double importance)
{
    return exact(u"Life regen"_s, u"#% increased Life Regeneration rate"_s,
                 PerSlot({{u"Jewel"_s, importance}}), 10);
}

// Life flasks are a belt prefix, and jewels roll the same thing.
ProfileStat flaskLifeRecovery(double importance)
{
    return stat(u"Flask life recovery"_s,
                uR"(^#% increased Flask Life Recovery rate$|^#% increased Life Recovery from Flasks$)"_s,
                PerSlot({{u"Belt"_s, importance}, {u"Jewel"_s, importance}}),
                PerSlot({{u"Jewel"_s, 15}}, 40));
}

// "#% increased <kind> Damage" for the kinds, and the plain "Damage" some
// jewels roll, plus the attack versions with their own wording.
QString damagePattern(const QString &kinds)
{
    return u"^#% increased (("_s + kinds + u") )?Damage$|^#% increased Elemental Damage with Attacks$"_s;
}

// What every character wants: life, resistances, attributes, and movement
// speed on boots.
QList<ProfileStat> common(double lifeImportance)
{
    return {
        life(lifeImportance),
        resistance(Element::Fire, 5, 45),
        resistance(Element::Cold, 5, 45),
        resistance(Element::Lightning, 5, 45),
        resistance(Element::Chaos, 3, 30),
        movementSpeed(),
        stat(u"Attributes"_s, uR"(^# to (Strength|Dexterity|Intelligence)$)"_s, PerSlot({{u"Jewel"_s, 0}}, 2),
             35),
        rarity(),
        stunThreshold(),
        percentStunThreshold(),
    };
}

BuildProfile lifeAndResistances()
{
    BuildProfile p{
        defaultBuildProfile.toString(),
        u"Life and resistances"_s,
        u"A safe all-rounder for any build: life, resistances, Armour or Evasion and movement speed. "
        "Weapons are rated by damage per second."_s,
        {
            weaponDps(),
            stat(u"Armour / Evasion"_s, uR"(^\[Base\] (Armour|Evasion Rating)$)"_s,
                 PerSlot({{u"Body Armour"_s, 10},
                          {u"Helmet"_s, 8},
                          {u"Gloves"_s, 7},
                          {u"Boots"_s, 6},
                          {u"Offhand"_s, 7}}),
                 PerSlot({{u"Body Armour"_s, 1200},
                          {u"Helmet"_s, 600},
                          {u"Gloves"_s, 400},
                          {u"Boots"_s, 450},
                          {u"Offhand"_s, 450}},
                         1)),
            baseEnergyShield(),
        },
    };
    p.stats += common(7);
    p.stats << stat(
        u"Speed"_s, uR"(^#% increased (Attack|Cast|Skill) Speed$)"_s,
        PerSlot({{u"Weapon"_s, 4}, {u"Ring"_s, 3}, {u"Amulet"_s, 3}, {u"Gloves"_s, 3}, {u"Jewel"_s, 3}}),
        PerSlot({{u"Jewel"_s, 6}}, 20));
    // Jewels and jewellery have little else a life build counts, so damage and
    // life percent count a little there. Hybrid builds get half of the Energy
    // Shield a spellcaster would.
    p.stats << stat(
        u"Damage"_s,
        damagePattern(u"Physical|Elemental|Fire|Cold|Lightning|Chaos|Attack|Melee|Projectile|Spell"_s),
        PerSlot({{u"Jewel"_s, 4}, {u"Ring"_s, 2}, {u"Amulet"_s, 2}}),
        PerSlot({{u"Jewel"_s, 15}, {u"Ring"_s, 30}}, 100));
    p.stats << exact(u"% Life"_s, u"#% increased maximum Life"_s,
                     PerSlot({{u"Jewel"_s, 6}, {u"Amulet"_s, 3}}), PerSlot({{u"Jewel"_s, 8}}, 15));
    p.stats << flatEnergyShield(0.5) << percentEnergyShield(0.5);
    p.stats << lifeRegeneration(2) << percentLifeRegeneration(2) << flaskLifeRecovery(1.5);
    return p;
}

BuildProfile energyShieldCaster()
{
    BuildProfile p{
        energyShieldProfile.toString(),
        u"Energy Shield spellcaster"_s,
        u"Spell skill levels, spell and elemental damage, cast speed and critical hits on weapons. "
        "Energy Shield and resistances on armour."_s,
        {
            stat(u"Spell skill levels"_s,
                 uR"(^# to Level of all (Spell|Fire Spell|Cold Spell|Lightning Spell|Chaos Spell) Skills$)"_s,
                 PerSlot({{u"Weapon"_s, 10}, {u"Offhand"_s, 9}, {u"Amulet"_s, 9}}),
                 PerSlot({{u"Weapon"_s, 5}}, 3), u"# to Level of all Spell Skills"_s),
            stat(
                u"Spell damage"_s,
                uR"(^#% increased ((Spell|Fire|Cold|Lightning|Chaos|Elemental) )?Damage$)"_s,
                PerSlot(
                    {{u"Weapon"_s, 9}, {u"Offhand"_s, 8}, {u"Amulet"_s, 5}, {u"Ring"_s, 4}, {u"Jewel"_s, 7}}),
                PerSlot({{u"Jewel"_s, 15}, {u"Ring"_s, 30}}, 100), u"#% increased Spell Damage"_s),
            stat(u"Cast speed"_s, uR"(^#% increased (Cast|Skill) Speed$)"_s,
                 PerSlot({{u"Weapon"_s, 7}, {u"Ring"_s, 5}, {u"Amulet"_s, 5}, {u"Jewel"_s, 4}}),
                 PerSlot({{u"Jewel"_s, 6}}, 25), u"#% increased Cast Speed"_s),
            stat(u"Spell critical hit chance and damage bonus"_s,
                 uR"(^#% increased Critical (Hit Chance( for Spells)?|(Spell )?Damage Bonus)$)"_s,
                 PerSlot({{u"Weapon"_s, 6},
                          {u"Offhand"_s, 5},
                          {u"Amulet"_s, 4},
                          {u"Jewel"_s, 4},
                          {u"Helmet"_s, 3}}),
                 PerSlot({{u"Jewel"_s, 15}}, 60)),
            baseEnergyShield(),
            percentEnergyShield(),
            flatEnergyShield(),
            extraEnergyShield(),
            mana(),
            spirit(5),
            energyShieldRecharge(2),
            energyShieldRechargeStart(2),
            manaRegeneration(1.5),
        },
    };
    p.stats += common(3);
    return p;
}

BuildProfile energyShieldAttacker()
{
    BuildProfile p{
        energyShieldAttackerProfile.toString(),
        u"Energy Shield attacker"_s,
        u"Weapon damage, attack speed, critical hits and added damage to attacks, with Energy Shield, "
        "resistances and movement speed to stay alive."_s,
        {
            weaponDps(),
            stat(u"Attack skill levels"_s, uR"(^# to Level of all (Attack|Melee|Projectile) Skills$)"_s,
                 PerSlot({{u"Amulet"_s, 8}, {u"Weapon"_s, 6}, {u"Offhand"_s, 4}}),
                 PerSlot({{u"Weapon"_s, 5}}, 3)),
            stat(u"Attack speed"_s, uR"(^#% increased (Attack|Skill) Speed$)"_s,
                 PerSlot({{u"Weapon"_s, 7},
                          {u"Gloves"_s, 6},
                          {u"Ring"_s, 5},
                          {u"Amulet"_s, 5},
                          {u"Jewel"_s, 4},
                          {u"Offhand"_s, 3}}),
                 PerSlot({{u"Jewel"_s, 6}}, 25), u"#% increased Attack Speed"_s),
            stat(u"Attack critical hit chance and damage bonus"_s,
                 uR"(^#% increased Critical (Hit Chance( for Attacks)?|Damage Bonus( for Attack Damage)?)$|)"
                 "^#% to Critical Damage Bonus$"_s,
                 PerSlot({{u"Weapon"_s, 6},
                          {u"Amulet"_s, 5},
                          {u"Offhand"_s, 4},
                          {u"Jewel"_s, 4},
                          {u"Helmet"_s, 3}}),
                 PerSlot({{u"Jewel"_s, 15}}, 60)),
            stat(u"Base critical hit chance"_s, uR"(^#% to Critical Hit Chance$)"_s,
                 PerSlot({{u"Weapon"_s, 5}}), 3.5),
            stat(u"Attack damage"_s,
                 damagePattern(u"Physical|Elemental|Fire|Cold|Lightning|Chaos|Attack|Melee|Projectile"_s) +
                     u"|^#% increased Damage with (Bows|Bow Skills|Crossbows|Maces|Spears|Quarterstaves)$"_s,
                 PerSlot({{u"Jewel"_s, 7},
                          {u"Amulet"_s, 6},
                          {u"Ring"_s, 5},
                          {u"Gloves"_s, 4},
                          {u"Offhand"_s, 3},
                          {u"Weapon"_s, 2}}),
                 PerSlot({{u"Jewel"_s, 15}, {u"Ring"_s, 30}, {u"Weapon"_s, 150}}, 100)),
            // on a weapon these are local, so its damage per second has them already
            stat(u"Added damage"_s,
                 uR"((?i)^Adds # to # (Physical|Fire|Cold|Lightning|Chaos) Damage( to Attacks)?$)"_s,
                 PerSlot({{u"Gloves"_s, 8},
                          {u"Ring"_s, 7},
                          {u"Amulet"_s, 5},
                          {u"Offhand"_s, 3},
                          {u"Weapon"_s, 3}}),
                 PerSlot({{u"Weapon"_s, 50}}, 20)),
            exact(
                u"Accuracy"_s, u"# to Accuracy Rating"_s,
                PerSlot(
                    {{u"Helmet"_s, 3}, {u"Gloves"_s, 3}, {u"Ring"_s, 3}, {u"Amulet"_s, 3}, {u"Weapon"_s, 2}}),
                400),
            exact(u"% Accuracy"_s, u"#% increased Accuracy Rating"_s,
                  PerSlot({{u"Jewel"_s, 4}, {u"Amulet"_s, 3}, {u"Gloves"_s, 2}}),
                  PerSlot({{u"Jewel"_s, 15}}, 30)),
            baseEnergyShield(),
            percentEnergyShield(),
            flatEnergyShield(),
            extraEnergyShield(),
            // a lesser defence, for bases that have both
            stat(u"Evasion"_s, uR"(^\[Base\] Evasion Rating$)"_s,
                 PerSlot({{u"Body Armour"_s, 4},
                          {u"Helmet"_s, 3},
                          {u"Gloves"_s, 3},
                          {u"Boots"_s, 3},
                          {u"Offhand"_s, 3}}),
                 PerSlot({{u"Body Armour"_s, 1200},
                          {u"Helmet"_s, 600},
                          {u"Gloves"_s, 400},
                          {u"Boots"_s, 450},
                          {u"Offhand"_s, 450}},
                         1)),
            mana(),
            spirit(4),
            energyShieldRecharge(2),
            energyShieldRechargeStart(2),
        },
    };
    p.stats += common(3);
    return p;
}

BuildProfile minionSummoner()
{
    const QString allies = u"(Allies in your Presence|Minions)"_s;
    return {
        minionProfile.toString(),
        u"Minion summoner on Energy Shield"_s,
        u"Minion skill levels and Spirit first, then ally and minion damage, Energy Shield and "
        "resistances. Movement speed on boots."_s,
        {
            exact(u"Minion skill levels"_s, u"# to Level of all Minion Skills"_s,
                  PerSlot({{u"Weapon"_s, 10}, {u"Offhand"_s, 10}, {u"Amulet"_s, 10}, {u"Helmet"_s, 9}}),
                  PerSlot({{u"Weapon"_s, 4}, {u"Offhand"_s, 3}, {u"Amulet"_s, 3}}, 2)),
            stat(u"Spirit (base)"_s, uR"(^\[Base\] Spirit$)"_s, PerSlot({{u"Weapon"_s, 8}}), 160),
            exact(u"Spirit"_s, u"# to Spirit"_s,
                  PerSlot({{u"Body Armour"_s, 8}, {u"Amulet"_s, 8}, {u"Weapon"_s, 7}}), 60),
            exact(u"Ally damage"_s, u"Allies in your Presence deal #% increased Damage"_s,
                  PerSlot({{u"Weapon"_s, 8}}), 90),
            stat(u"Ally extra damage"_s, uR"(^Allies in your Presence Gain #% of Damage as Extra)"_s,
                 PerSlot({{u"Weapon"_s, 6}}), 15),
            // also the conditional versions, which the gear score counts for less
            stat(u"Minion damage"_s, uR"(^Minions deal #% increased Damage)"_s,
                 PerSlot({{u"Ring"_s, 7}, {u"Weapon"_s, 6}, {u"Offhand"_s, 6}, {u"Jewel"_s, 7}}),
                 PerSlot({{u"Ring"_s, 25}, {u"Jewel"_s, 15}}, 45), u"Minions deal #% increased Damage"_s),
            stat(u"Minion critical hit chance and damage bonus"_s,
                 u'^' + allies + uR"( have #% increased Critical (Hit Chance|Damage Bonus)$)"_s,
                 PerSlot({{u"Weapon"_s, 5}, {u"Ring"_s, 5}, {u"Jewel"_s, 5}}),
                 PerSlot({{u"Jewel"_s, 20}}, 40)),
            stat(u"Minion speed"_s,
                 uR"(^(Allies in your Presence|Minions|You and Allies in your Presence) have #% increased )"
                 "(Attack Speed|Cast Speed|Attack and Cast Speed)$"_s,
                 PerSlot({{u"Weapon"_s, 5}, {u"Ring"_s, 4}, {u"Amulet"_s, 4}, {u"Jewel"_s, 5}}),
                 PerSlot({{u"Jewel"_s, 8}}, 20)),
            stat(u"Minion res"_s, u'^' + allies + u" have #% to all Elemental Resistances$"_s,
                 PerSlot({{u"Weapon"_s, 2}}), 16),
            stat(u"Minion defence"_s,
                 uR"(^Minions have #% increased maximum Life$|)"
                 "^Minions gain #% of their maximum Life as Extra maximum Energy Shield$"_s,
                 PerSlot({{u"Weapon"_s, 2}, {u"Offhand"_s, 2}, {u"Jewel"_s, 2}}),
                 PerSlot({{u"Jewel"_s, 10}}, 30)),
            stat(u"Spirit efficiency"_s, uR"(^#% increased Spirit Reservation Efficiency of Skills$)"_s,
                 PerSlot({{u"Amulet"_s, 5}}), 15),
            baseEnergyShield(),
            stat(u"Armour"_s, uR"(^\[Base\] Armour$)"_s, PerSlot({{u"Body Armour"_s, 2}}), 800),
            flatEnergyShield(),
            percentEnergyShield(),
            extraEnergyShield(),
            life(4),
            mana(),
            resistance(Element::Fire, 4, 45),
            resistance(Element::Cold, 4, 45),
            resistance(Element::Lightning, 4, 45),
            resistance(Element::Chaos, 4, 30),
            exact(u"Intelligence"_s, u"# to Intelligence"_s, PerSlot({{u"Jewel"_s, 1}}, 3),
                  PerSlot({{u"Jewel"_s, 12}}, 35)),
            stat(u"Str/Dex"_s, uR"(^# to (Strength|Dexterity)$)"_s, PerSlot({{u"Jewel"_s, 0}}, 1.5), 35),
            movementSpeed(),
            exact(u"Cast speed"_s, u"#% increased Cast Speed"_s,
                  PerSlot({{u"Ring"_s, 3}, {u"Amulet"_s, 3}, {u"Offhand"_s, 3}, {u"Jewel"_s, 2}}),
                  PerSlot({{u"Jewel"_s, 6}}, 25)),
            energyShieldRecharge(2),
            energyShieldRechargeStart(2),
            manaRegeneration(1.5),
            lifeRegeneration(1),
            percentLifeRegeneration(1),
            stunThreshold(),
            percentStunThreshold(),
            rarity(),
        },
    };
}

} // namespace

QString resistLabel(Element e)
{
    return elementName(e) + u" res"_s;
}

double PerSlot::at(const QString &slotGroup) const
{
    for (const auto &[slot, value] : m_overrides) {
        if (slot == slotGroup)
            return value;
    }
    return m_elsewhere;
}

const QList<BuildProfile> &buildProfiles()
{
    static const QList<BuildProfile> profiles = {lifeAndResistances(), energyShieldCaster(),
                                                 energyShieldAttacker(), minionSummoner()};
    return profiles;
}

const BuildProfile *buildProfile(QStringView id)
{
    if (id.isEmpty())
        id = defaultBuildProfile;
    for (const BuildProfile &p : buildProfiles()) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

const QStringList &ratedGroups()
{
    static const QStringList groups = {
        u"Weapon"_s, u"Offhand"_s, u"Helmet"_s, u"Body Armour"_s, u"Gloves"_s,
        u"Boots"_s,  u"Belt"_s,    u"Amulet"_s, u"Ring"_s,        u"Jewel"_s,
    };
    return groups;
}

QList<TweakableStat> tweakableStats(const BuildProfile &profile)
{
    QList<TweakableStat> out;
    for (const ProfileStat &s : profile.stats) {
        if (std::ranges::any_of(out, [&s](const TweakableStat &t) { return t.label == s.label; }))
            continue;
        TweakableStat tweakable{.label = s.label, .groups = {}};
        for (const QString &group : ratedGroups()) {
            // where any of the stats with the label counts
            const bool counts = std::ranges::any_of(profile.stats, [&](const ProfileStat &other) {
                return other.label == s.label && other.importance.at(group) > 0;
            });
            if (counts)
                tweakable.groups << group;
        }
        out << tweakable;
    }
    return out;
}

} // namespace pou
