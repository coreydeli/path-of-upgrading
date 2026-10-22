// SPDX-License-Identifier: MIT
#include "core/buildprofiles.h"

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
    ProfileStat s = exact(elementName(e) + u" res"_s, resistKey(e),
                          PerSlot({{u"Weapon"_s, 0}, {u"Jewel"_s, 0}}, importance), cap);
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
ProfileStat flatEnergyShield()
{
    return exact(u"Energy Shield"_s, u"# to maximum Energy Shield"_s,
                 PerSlot({{u"Amulet"_s, 6}, {u"Ring"_s, 5}, {u"Belt"_s, 4}}), 60);
}

ProfileStat percentEnergyShield()
{
    return exact(u"% Energy Shield"_s, u"#% increased maximum Energy Shield"_s,
                 PerSlot({{u"Amulet"_s, 7}, {u"Jewel"_s, 7}}), PerSlot({{u"Jewel"_s, 12}}, 25));
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

ProfileStat mana()
{
    return exact(u"Mana"_s, u"# to maximum Mana"_s, PerSlot({{u"Jewel"_s, 0}}, 3), 150);
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
    };
}

BuildProfile lifeAndResistances()
{
    BuildProfile p{
        u"life-res"_s,
        u"Life and resistances"_s,
        u"A safe all-rounder for any build: life, resistances, Armour or Evasion and movement speed. "
        "Weapons are rated by damage per second."_s,
        {
            stat(u"Weapon DPS"_s, uR"(^\[Weapon\] Total DPS$)"_s, PerSlot({{u"Weapon"_s, 10}}), 400),
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
    p.stats << stat(u"Speed"_s, uR"(^#% increased (Attack|Cast) Speed$)"_s,
                    PerSlot({{u"Weapon"_s, 4}, {u"Ring"_s, 3}, {u"Amulet"_s, 3}, {u"Gloves"_s, 3}}), 20);
    return p;
}

BuildProfile energyShieldCaster()
{
    BuildProfile p{
        u"es-caster"_s,
        u"Energy Shield spellcaster"_s,
        u"Spell skill levels, spell and elemental damage, cast speed and critical hits on weapons. "
        "Energy Shield and resistances on armour."_s,
        {
            stat(u"Spell skill levels"_s,
                 uR"(^# to Level of all (Spell|Fire Spell|Cold Spell|Lightning Spell|Chaos Spell) Skills$)"_s,
                 PerSlot({{u"Weapon"_s, 10}, {u"Offhand"_s, 9}, {u"Amulet"_s, 9}}),
                 PerSlot({{u"Weapon"_s, 5}}, 3), u"# to Level of all Spell Skills"_s),
            stat(
                u"Spell damage"_s, uR"(^#% increased (Spell|Fire|Cold|Lightning|Chaos|Elemental) Damage$)"_s,
                PerSlot(
                    {{u"Weapon"_s, 9}, {u"Offhand"_s, 8}, {u"Amulet"_s, 5}, {u"Ring"_s, 4}, {u"Jewel"_s, 7}}),
                PerSlot({{u"Jewel"_s, 15}, {u"Ring"_s, 30}}, 100), u"#% increased Spell Damage"_s),
            exact(u"Cast speed"_s, u"#% increased Cast Speed"_s,
                  PerSlot({{u"Weapon"_s, 7}, {u"Ring"_s, 5}, {u"Amulet"_s, 5}, {u"Jewel"_s, 4}}),
                  PerSlot({{u"Jewel"_s, 6}}, 25)),
            stat(u"Crit"_s, uR"(^#% increased Critical (Hit Chance|Damage Bonus)( for Spells)?$)"_s,
                 PerSlot({{u"Weapon"_s, 6},
                          {u"Offhand"_s, 5},
                          {u"Amulet"_s, 4},
                          {u"Jewel"_s, 4},
                          {u"Helmet"_s, 3}}),
                 PerSlot({{u"Jewel"_s, 15}}, 60)),
            baseEnergyShield(),
            percentEnergyShield(),
            flatEnergyShield(),
            mana(),
            exact(u"Spirit"_s, u"# to Spirit"_s, PerSlot({{u"Body Armour"_s, 5}, {u"Amulet"_s, 5}}), 60),
        },
    };
    p.stats += common(3);
    return p;
}

BuildProfile minionSummoner()
{
    const QString allies = u"(Allies in your Presence|Minions)"_s;
    return {
        u"minion-es"_s,
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
            stat(u"Minion crit"_s,
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
            stat(u"Extra ES"_s, uR"(^Gain #% of maximum (Life|Mana) as Extra maximum Energy Shield$)"_s,
                 PerSlot({{u"Body Armour"_s, 5}}), 15),
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
            stat(
                u"ES recharge"_s,
                uR"(^#% increased Energy Shield Recharge Rate$|^#% faster start of Energy Shield Recharge$)"_s,
                PerSlot({{u"Offhand"_s, 2},
                         {u"Body Armour"_s, 2},
                         {u"Helmet"_s, 2},
                         {u"Belt"_s, 2},
                         {u"Amulet"_s, 2},
                         {u"Jewel"_s, 3}}),
                PerSlot({{u"Jewel"_s, 25}}, 40)),
            stat(u"Mana regen"_s, uR"(^#% increased Mana Regeneration Rate$)"_s,
                 PerSlot({{u"Ring"_s, 1.5}, {u"Amulet"_s, 1.5}, {u"Offhand"_s, 1.5}}), 40),
            stat(u"Life regen"_s, uR"(^# Life Regeneration per second$)"_s, PerSlot({{u"Jewel"_s, 0}}, 1),
                 15),
            rarity(),
        },
    };
}

} // namespace

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
                                                 minionSummoner()};
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

} // namespace pou
