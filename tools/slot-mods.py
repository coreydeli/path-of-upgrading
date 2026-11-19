#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Makes data/slotmods.json from Path of Building's Path of Exile 2 data.

Run it by hand after a patch, with the data folder and where to write:

    python3 -I tools/slot-mods.py <PathOfBuilding2 folder> data/slotmods.json

The folder is the one with Data/ModItem.lua and Data/Bases/ in it (the
Flatpak keeps it under ~/.var/app/community.pathofbuilding.PathOfBuilding/
data/). Nothing is downloaded.

For each item class it lists the prefixes and suffixes a base of that class
can roll, by the stat text with its numbers taken out, as the app keys item
stats, and each one's tiers as [item level, low, high], with a 1 on the end for
those of hybrid mods (more than one stat), whose ranges are their own.

It also says whether each stat is a prefix ("p") or a suffix ("s") on that
class, under "kinds". A stat that is a prefix on one mod and a suffix on
another of the same class is left out, as nothing says which one an item has.
"""
import datetime
import json
import re
import sys
from pathlib import Path

# Path of Building's base types and the item class the game calls them.
PLAIN = {
    "Helmet": "Helmets",
    "Body Armour": "Body Armours",
    "Gloves": "Gloves",
    "Boots": "Boots",
    "Belt": "Belts",
    "Amulet": "Amulets",
    "Ring": "Rings",
    "Focus": "Foci",
    "Quiver": "Quivers",
    "Bow": "Bows",
    "Crossbow": "Crossbows",
    "Claw": "Claws",
    "Dagger": "Daggers",
    "Flail": "Flails",
    "Spear": "Spears",
    "Sceptre": "Sceptres",
    "Talisman": "Talismans",
    "Wand": "Wands",
    "One Hand Axe": "One Hand Axes",
    "Two Hand Axe": "Two Hand Axes",
    "One Hand Mace": "One Hand Maces",
    "Two Hand Mace": "Two Hand Maces",
    "One Hand Sword": "One Hand Swords",
    "Two Hand Sword": "Two Hand Swords",
}

BASE = re.compile(r'^itemBases\["[^"]+"\] = \{(.*?)^\}', re.S | re.M)
MOD = re.compile(r'^\t\["[^"]+"\] = \{ type = "(Prefix|Suffix)", .*$', re.M)
RANGE = re.compile(r"\((-?\d+(?:\.\d+)?)-(-?\d+(?:\.\d+)?)\)")
# a tier with one value, like "10% increased Movement Speed"
FIXED = re.compile(r"(?<![\w.])(\d+(?:\.\d+)?)(?![\w.])")


# two numbers or ranges joined by "to", counted as one stat
PAIR = re.compile(
    r"(?P<lo>\((?:-?\d+(?:\.\d+)?)-(?:-?\d+(?:\.\d+)?)\)|\d+(?:\.\d+)?) to "
    r"(?P<hi>\((?:-?\d+(?:\.\d+)?)-(?:-?\d+(?:\.\d+)?)\)|\d+(?:\.\d+)?)(?![\w.%])"
)


def fold(key):
    """The key the app gives a stat, which writes reduced as increased and less as more."""
    return re.sub(r"\bless\b", "more", re.sub(r"\breduced\b", "increased", key))


def item_class(kind, tags):
    if kind == "Shield":
        return "Bucklers" if "buckler" in tags else "Shields"
    if kind == "Staff":
        return "Quarterstaves" if "warstaff" in tags else "Staves"
    return PLAIN.get(kind)


def read_bases(folder):
    """Every base's item class and tags."""
    bases = []
    for path in sorted((folder / "Data" / "Bases").glob("*.lua")):
        for body in BASE.findall(path.read_text(encoding="utf-8")):
            kind = re.search(r'type = "([^"]+)"', body)
            tags = re.search(r"tags = \{([^}]*)\}", body)
            if not kind or not tags:
                continue
            names = set(re.findall(r"(\w+) = true", tags.group(1)))
            # hidden and unique-only bases don't set what a slot can roll
            if "hidden = true" in body or "not_for_sale" in names:
                continue
            cls = item_class(kind.group(1), names)
            if cls:
                bases.append((cls, names))
    return bases


def weight_for(keys, weights, tags):
    """The weight of the first of the mod's tags that the base has."""
    for key, weight in zip(keys, weights):
        # these come only from the Genesis tree, not from rolling the item
        if key.startswith("genesis_tree"):
            continue
        if key == "default" or key in tags:
            return weight
    return 0


def read_mods(folder):
    mods = []
    for line in MOD.finditer((folder / "Data" / "ModItem.lua").read_text(encoding="utf-8")):
        text = line.group(0)
        kind = "p" if line.group(1) == "Prefix" else "s"
        level = int(re.search(r"level = (\d+)", text).group(1))
        keys = re.search(r"weightKey = \{([^}]*)\}", text).group(1)
        weights = re.search(r"weightVal = \{([^}]*)\}", text).group(1)
        keys = re.findall(r'"([^"]+)"', keys)
        weights = [int(w) for w in re.findall(r"\d+", weights)]
        # the stat lines come before the other fields, as bare strings
        head = text.split("statOrder", 1)[0]
        lines = re.findall(r'"([^"]+)"', head.split("affix =", 1)[1])[1:]
        stats = []
        for stat in lines:
            pair = PAIR.search(stat)
            if pair:
                # "Adds (1-3) to (5-8) Fire Damage": the app counts the average
                low = [float(x) for x in pair.group("lo").strip("()").split("-")]
                high = [float(x) for x in pair.group("hi").strip("()").split("-")]
                key = fold(stat[: pair.start()] + "# to #" + stat[pair.end() :]).lstrip("+")
                stats.append((key, (low[0] + high[0]) / 2, (low[-1] + high[-1]) / 2))
                continue
            ranges = RANGE.findall(stat)
            if len(ranges) == 1:
                key = fold(RANGE.sub("#", stat).lstrip("+"))
                stats.append((key, float(ranges[0][0]), float(ranges[0][1])))
                continue
            fixed = FIXED.findall(stat)
            if not ranges and len(fixed) == 1:
                key = fold(FIXED.sub("#", stat).lstrip("+"))
                stats.append((key, float(fixed[0]), float(fixed[0])))
        if stats:
            # a hybrid mod gives more than one stat, and its ranges are its own
            mods.append((level, keys, weights, stats, len(lines) > 1, kind))
    return mods


def tier(found):
    """[item level, low, high], with a 1 on the end for a hybrid mod's."""
    level, low, high, hybrid = found
    return [level, low, high, 1] if hybrid else [level, low, high]


def number(value):
    return int(value) if value == int(value) else value


def main(folder, out):
    folder = Path(folder)
    bases = read_bases(folder)
    mods = read_mods(folder)
    classes = {}
    kinds = {}
    for cls in sorted({cls for cls, _ in bases}):
        tag_sets = [tags for c, tags in bases if c == cls]
        found = {}
        seen = {}
        for level, keys, weights, stats, hybrid, kind in mods:
            if not any(weight_for(keys, weights, tags) > 0 for tags in tag_sets):
                continue
            for key, low, high in stats:
                found.setdefault(key, set()).add((level, number(low), number(high), hybrid))
                seen.setdefault(key, set()).add(kind)
        kinds[cls] = {key: next(iter(k)) for key, k in sorted(seen.items()) if len(k) == 1}
        classes[cls] = {key: [tier(t) for t in sorted(tiers, reverse=True)] for key, tiers in sorted(found.items())}

    version = re.search(r'number="([^"]+)"', (folder / "manifest.xml").read_text(encoding="utf-8"))
    table = {
        "source": "Path of Building (Path of Exile 2), Data/ModItem.lua and Data/Bases",
        "version": version.group(1) if version else "unknown",
        "date": datetime.date.today().isoformat(),
        "tiers": "[item level, low, high], the highest item level first, with a 1 on the end when the mod is a hybrid",
        "kindsNote": "p for a prefix and s for a suffix, by class and stat, left out when a class has the stat as both",
        "classes": classes,
        "kinds": kinds,
    }
    Path(out).write_text(json.dumps(table, separators=(",", ":"), ensure_ascii=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
