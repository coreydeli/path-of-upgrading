// SPDX-License-Identifier: MIT
// A build laid out the way Path of Building 2 saves it, for the import tests.
#pragma once

#include <QString>

using namespace Qt::Literals::StringLiterals;

namespace pobfixtures {

// A level 92 Stormweaver with two item sets and two passive trees, the second
// of each active. Its item set uses the second weapon set.
inline const QString build = uR"xml(<?xml version="1.0" encoding="UTF-8"?>
<PathOfBuilding2>
	<Build level="92" targetVersion="0_1" viewMode="ITEMS" className="Sorceress" ascendClassName="Stormweaver" mainSocketGroup="1" characterLevelAutoMode="false">
		<PlayerStat stat="Life" value="1630"/>
		<PlayerStat value="75" stat="FireResist"/>
		<PlayerStat value="18" stat="FireResistOverCap"/>
		<PlayerStat stat="ColdResist" value="62"/>
		<PlayerStat stat="ColdResistOverCap" value="0"/>
		<PlayerStat stat="LightningResist" value="75"/>
		<PlayerStat stat="LightningResistOverCap" value="4"/>
		<PlayerStat stat="ChaosResist" value="-10"/>
		<PlayerStat stat="ChaosResistOverCap" value="0"/>
		<MinionStat stat="FireResist" value="40"/>
	</Build>
	<Import lastLeague="Standard" lastRealm="PoE2" exportParty="false"/>
	<Tree activeSpec="2">
		<Spec title="Levelling" treeVersion="0_5" classId="3" ascendClassId="1" nodes="100,61834" secondaryAscendClassId="nil">
			<URL>
				https://www.pathofexile.com/passive-skill-tree/AAAABgMBAA==
			</URL>
			<Sockets>
				<Socket nodeId="61834" itemId="10"/>
			</Sockets>
			<Overrides/>
		</Spec>
		<Spec title="Endgame" treeVersion="0_5" classId="3" ascendClassId="1" nodes="100,4242,61834,21984" secondaryAscendClassId="nil">
			<URL>
				https://www.pathofexile.com/passive-skill-tree/AAAABgMBAQ==
			</URL>
			<Sockets>
				<Socket itemId="8" nodeId="61834"/>
				<Socket itemId="10" nodeId="32763"/>
				<Socket itemId="9" nodeId="21984"/>
			</Sockets>
			<Overrides/>
		</Spec>
	</Tree>
	<Notes>
		Swap to the staff for clearing.
	</Notes>
	<Items activeItemSet="2" useSecondWeaponSet="nil" showStatDifferences="true">
		<Item id="1">
			Rarity: RARE
Doom Vise
Sorcerer Gloves
Energy Shield: 98
Unique ID: 9f1c27ab
Item Level: 81
Quality: 20
Sockets: S S
Rune: Glacial Rune
Rune: None
LevelReq: 65
Implicits: 1
{rune}+12% to Cold Resistance
{tags:resistance}+39% to Lightning Resistance
{crafted}{range:0.5}+(30-36) to maximum Life
75% increased Energy Shield
			<ModRange range="0.5" id="3"/>
		</Item>
		<Item id="2">
			Rarity: RARE
Golem Cord
Heavy Belt
Charm Slots: 2
Unique ID: 3b8e0d44
Item Level: 80
Quality: 0
LevelReq: 56
Implicits: 1
{range:0.5}(20-30)% increased Stun Threshold
{range:1}+(25-35) to Strength
+44% to Fire Resistance
+19% to Chaos Resistance
		</Item>
		<Item id="3">
			Rarity: RARE
Victory Star
Omen Sceptre
Spirit: 100
Unique ID: 77c0e1f2
Item Level: 82
Quality: 0
Sockets: S
Rune: None
LevelReq: 76
Implicits: 1
Grants Skill: Level 19 Malice
+4 to Level of all Minion Skills
{range:0.5}(30-40)% increased Spirit
		</Item>
		<Item id="4">
			Rarity: RARE
Rapture Ward
Twig Focus
Energy Shield: 41
Unique ID: 0e9d6a13
Item Level: 78
Quality: 0
LevelReq: 45
Implicits: 0
{range:0.2}+(30-40) to maximum Energy Shield
+31% to Lightning Resistance
		</Item>
		<Item id="5">
			Rarity: RARE
Corpse Song
Ashen Staff
Unique ID: c4a2b9e0
Item Level: 70
Quality: 0
LevelReq: 52
Implicits: 1
Grants Skill: Level 15 Firebolt
+3 to Level of all Fire Spell Skills
		</Item>
		<Item id="6">
			Rarity: NORMAL
Ultimate Life Flask
Unique ID: 5a5e7c21
Item Level: 60
Quality: 20
LevelReq: 50
Implicits: 0
		</Item>
		<Item id="7">
			Rarity: RARE
Omen Loop
Gold Ring
Unique ID: 81d3f0aa
Item Level: 75
LevelReq: 40
Implicits: 1
{range:0.5}(6-15)% increased Rarity of Items found
+28% to Cold Resistance
		</Item>
		<Item id="8">
			Rarity: RARE
Grim Hope
Sapphire
Unique ID: 2f6b9d70
Item Level: 79
Implicits: 0
{range:0.5}(10-20)% increased Critical Hit Chance for Spells
14% increased Energy Shield
		</Item>
		<Item id="9">
			Rarity: RARE
Rift Eye
Emerald
Unique ID: 6c1e4b8d
Item Level: 81
Implicits: 0
{range:1}(5-10)% increased Projectile Speed
		</Item>
		<Item id="10">
			Rarity: RARE
Woe Spark
Ruby
Unique ID: d90a3e57
Item Level: 68
Implicits: 0
8% increased maximum Life
		</Item>
		<Item id="11">
			Rarity: NORMAL
Twig Circlet
Energy Shield: 18
Unique ID: 4e7f2c19
Item Level: 12
LevelReq: 5
Implicits: 0
		</Item>
		<ItemSet useSecondWeaponSet="nil" title="Levelling" id="1">
			<Slot itemId="11" name="Helmet" itemPbURL=""/>
			<Slot itemId="0" name="Gloves" itemPbURL=""/>
			<Slot itemId="2" name="Belt" itemPbURL=""/>
		</ItemSet>
		<ItemSet useSecondWeaponSet="true" title="Endgame" id="2">
			<Slot itemId="5" name="Weapon 1" itemPbURL=""/>
			<Slot itemId="0" name="Weapon 2" itemPbURL=""/>
			<Slot itemId="3" name="Weapon 1 Swap" itemPbURL=""/>
			<Slot itemId="4" name="Weapon 2 Swap" itemPbURL=""/>
			<Slot itemId="0" name="Helmet" itemPbURL=""/>
			<Slot itemId="1" name="Gloves" itemPbURL=""/>
			<Slot itemId="2" name="Belt" itemPbURL=""/>
			<Slot itemId="99" name="Ring 1" itemPbURL=""/>
			<Slot itemId="7" name="Ring 3" itemPbURL=""/>
			<Slot itemId="6" name="Flask 1" itemPbURL="" active="true"/>
			<Slot itemId="0" name="Leg 1" itemPbURL=""/>
			<SocketIdURL nodeId="61834" name="Jewel 61834" itemPbURL=""/>
		</ItemSet>
		<TradeSearchWeights>
			<Stat weightMult="1.00" stat="FullDPS" label="Full DPS"/>
		</TradeSearchWeights>
	</Items>
	<Config activeConfigSet="1">
		<ConfigSet title="Default" id="1">
			<Input name="usePowerCharges" boolean="true"/>
		</ConfigSet>
	</Config>
</PathOfBuilding2>
)xml"_s;

} // namespace pobfixtures
