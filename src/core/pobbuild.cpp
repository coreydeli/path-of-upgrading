// SPDX-License-Identifier: MIT
#include "core/pobbuild.h"

#include "core/numbers.h"
#include "core/slots.h"

#include <QByteArray>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QXmlStreamReader>
#include <algorithm>
#include <array>
#include <cmath>
#include <zlib.h>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// A build is a few hundred kB of XML, so whatever inflates past this isn't one.
constexpr qsizetype maxXmlSize = 32 * 1024 * 1024;

// windowBits as zlib takes them: 15 + 32 reads zlib or gzip data, -15 raw deflate.
std::optional<QByteArray> inflated(const QByteArray &data, int windowBits)
{
    z_stream stream{};
    if (data.size() > maxXmlSize || inflateInit2(&stream, windowBits) != Z_OK)
        return std::nullopt;

    QByteArray out;
    char chunk[16384];
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    stream.avail_in = uInt(data.size());
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef *>(chunk);
        stream.avail_out = sizeof chunk;
        status = inflate(&stream, Z_NO_FLUSH);
        out.append(chunk, qsizetype(sizeof chunk - stream.avail_out));
        if (out.size() > maxXmlSize)
            status = Z_MEM_ERROR;
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END)
        return std::nullopt;
    return out;
}

// The base properties Path of Building writes, as the game names them.
QString propertyName(const QString &name)
{
    static const QHash<QString, QString> names = {
        {u"Armour"_s, u"Armour"_s},
        {u"Evasion"_s, u"Evasion Rating"_s},
        {u"Energy Shield"_s, u"Energy Shield"_s},
        {u"Ward"_s, u"Runic Ward"_s},
        {u"Spirit"_s, u"Spirit"_s},
    };
    return names.value(name);
}

// What Path of Building keeps about an item that the game doesn't show, or
// that the item text gets across some other way.
bool isMetadata(const QString &name)
{
    static const QSet<QString> names = {
        u"Unique ID"_s,
        u"League"_s,
        u"Unreleased"_s,
        u"Crafted"_s,
        u"Prefix"_s,
        u"Suffix"_s,
        u"Catalyst"_s,
        u"CatalystQuality"_s,
        u"Quality"_s,
        u"Sockets"_s,
        u"Rune"_s,
        u"Radius"_s,
        u"Limited to"_s,
        u"Charm Slots"_s,
        u"Talisman Tier"_s,
        u"Source"_s,
        u"Note"_s,
        u"Has Variants"_s,
        u"Selected Variants"_s,
        u"Implicit"_s,
        u"Upgrade"_s,
        u"Requires"_s,
        u"Item Class"_s,
        u"Cluster Jewel Skill"_s,
        u"Cluster Jewel Node Count"_s,
    };
    // "Quality (Attack Modifiers)" and the like
    return names.contains(name) || name.startsWith(u"Quality ");
}

// Lines after the item's name that only say what state it's in.
bool isState(const QString &line)
{
    return line == u"Corrupted" || line == u"Twice Corrupted" || line == u"Mirrored" ||
           line == u"Sanctified" || line == u"Unidentified";
}

QString capitalized(const QString &word)
{
    return word.left(1).toUpper() + word.mid(1).toLower();
}

// "+(30-36) to maximum Life" at a roll of 0.5 is "+33 to maximum Life", the
// way Path of Building reads its ranges. Ranges with decimals keep two, the
// most the game shows.
QString rolled(const QString &line, double roll)
{
    static const QRegularExpression range(uR"(([+-]?)\((-?\d+(?:\.\d*)?)-(-?\d+(?:\.\d*)?)\))"_s);
    // "-10% increased" reads "10% reduced"
    static const QRegularExpression negative(uR"(-(\d+(?:\.\d+)?%) (increased|reduced|more|less)\b)"_s);
    static const QHash<QString, QString> opposite = {
        {u"increased"_s, u"reduced"_s},
        {u"reduced"_s, u"increased"_s},
        {u"more"_s, u"less"_s},
        {u"less"_s, u"more"_s},
    };

    QString out;
    qsizetype done = 0;
    for (const QRegularExpressionMatch &m : range.globalMatch(line)) {
        const double low = m.captured(2).toDouble();
        const double high = m.captured(3).toDouble();
        const bool decimals = m.captured(2).contains(u'.') || m.captured(3).contains(u'.');
        double value = roundTo(low + (high - low) * roll, decimals ? 2 : 0);
        if (m.captured(1) == u"-")
            value = -value;
        out += line.sliced(done, m.capturedStart() - done);
        if (m.captured(1) == u"+" && value > 0)
            out += u'+';
        out += formatNumber(value);
        done = m.capturedEnd();
    }
    out += line.sliced(done);

    for (QRegularExpressionMatch m = negative.match(out); m.hasMatch(); m = negative.match(out)) {
        out.replace(m.capturedStart(), m.capturedLength(),
                    m.captured(1) + u' ' + opposite.value(m.captured(2)));
    }
    return out;
}

// One mod line of an item.
struct PobMod
{
    QString text;
    QString tag;         // what the game puts after it: rune, implicit, crafted...
    QList<int> variants; // the variants it's on, empty when it's on all of them
};

// Path of Building's tags in front of a mod line, {crafted}{range:0.5}...,
// and the ones after it, " (crafted)".
PobMod readMod(const QString &line)
{
    static const QRegularExpression braces(uR"(\{([A-Za-z]*):?([^}]*)\})"_s);
    static const QRegularExpression after(
        uR"( \((crafted|fractured|desecrated|mutated|enchant|implicit|rune|custom|unscalable)\))"_s);

    PobMod mod;
    QSet<QString> flags;
    double roll = 0.5; // Path of Building's default for rolls it wasn't given
    for (const QRegularExpressionMatch &m : braces.globalMatch(line)) {
        const QString key = m.captured(1);
        if (key == u"range") {
            bool ok = false;
            if (const double r = m.captured(2).toDouble(&ok); ok)
                roll = r;
        } else if (key == u"variant") {
            for (const QString &id : m.captured(2).split(u',')) {
                bool ok = false;
                if (const int v = id.trimmed().toInt(&ok); ok)
                    mod.variants << v;
            }
        } else {
            flags.insert(key);
        }
    }
    QString text = QString(line).remove(braces);
    for (const QRegularExpressionMatch &m : after.globalMatch(text))
        flags.insert(m.captured(1));
    text.remove(after);
    mod.text = rolled(text.trimmed(), roll);

    for (const QString &tag : {u"rune"_s, u"enchant"_s, u"implicit"_s, u"crafted"_s, u"fractured"_s,
                               u"desecrated"_s, u"mutated"_s}) {
        if (flags.contains(tag)) {
            mod.tag = tag;
            break;
        }
    }
    return mod;
}

// The class for weapons and off-hands. Most base types end in their class's
// word, the others are listed. Earlier rules win: a Sword Spear is a spear.
QString weaponClass(const QString &baseType)
{
    struct Rule
    {
        QRegularExpression words;
        QString itemClass;
    };
    const auto rule = [](const QString &words, const QString &itemClass) {
        return Rule{
            QRegularExpression(u"\\b("_s + words + u")\\b"_s, QRegularExpression::CaseInsensitiveOption),
            itemClass};
    };
    static const QList<Rule> rules = {
        rule(u"Quarterstaff"_s, u"Quarterstaves"_s),
        rule(u"Staff"_s, u"Staves"_s),
        rule(u"Crossbow|Cannon"_s, u"Crossbows"_s),
        rule(u"Bow|Shortbow|Greatbow"_s, u"Bows"_s),
        rule(u"Spear"_s, u"Spears"_s),
        rule(u"Flail"_s, u"Flails"_s),
        rule(u"Claw"_s, u"Claws"_s),
        rule(u"Dagger|Knife|Shank|Dirk|Cinquedea|Mail Breaker|Ethereal Blade"_s, u"Daggers"_s),
        rule(u"Talisman"_s, u"Talismans"_s),
        rule(u"Sceptre"_s, u"Sceptres"_s),
        rule(u"Wand|Fork"_s, u"Wands"_s),
        rule(u"Focus"_s, u"Foci"_s),
        rule(u"Quiver"_s, u"Quivers"_s),
        rule(u"Buckler"_s, u"Bucklers"_s),
        rule(u"Shield|Targe|Defender|Fortress"_s, u"Shields"_s),
        rule(u"Greatsword|Greatblade|Longsword|Claymore|Keyblade|Two Handed"_s, u"Two Hand Swords"_s),
        rule(u"Greataxe|Halberd|Glaive"_s, u"Two Hand Axes"_s),
        rule(u"Greatclub|Greathammer|Greatmace|Maul|Sledge"_s, u"Two Hand Maces"_s),
        rule(u"Axe|Hatchet|Cleaver"_s, u"One Hand Axes"_s),
        rule(u"Mace|Club|Hammer|Forgehammer|Warpick|Pick|Morning Star"_s, u"One Hand Maces"_s),
        rule(u"Sword|Shortsword|Broadsword|Blade|Scimitar|Falchion|Cutlass|Messer"_s, u"One Hand Swords"_s),
        rule(u"Fishing Rod"_s, u"Fishing Rods"_s),
    };
    for (const Rule &r : rules) {
        if (r.words.match(baseType).hasMatch())
            return r.itemClass;
    }
    return {};
}

// Weapons of these classes attack, so they have damage of their own.
// "Weapons" is a base we don't know, which might.
bool hasDamage(const QString &itemClass)
{
    static const QSet<QString> classes = {
        u"Bows"_s,           u"Crossbows"_s,       u"Quarterstaves"_s,   u"Spears"_s,
        u"Flails"_s,         u"Claws"_s,           u"Daggers"_s,         u"Talismans"_s,
        u"One Hand Axes"_s,  u"One Hand Maces"_s,  u"One Hand Swords"_s, u"Two Hand Axes"_s,
        u"Two Hand Maces"_s, u"Two Hand Swords"_s, u"Fishing Rods"_s,    u"Weapons"_s,
    };
    return classes.contains(itemClass);
}

// Path of Building's slots and ours for them when its first weapon set is in
// use. It names its weapon sets "Weapon 1" and "Weapon 1 Swap"; ours are the
// set in use and the other one.
const QHash<QString, QString> &slotsByPobSlot()
{
    static const QHash<QString, QString> slots = {
        {u"Weapon 1"_s, u"Weapon"_s},
        {u"Weapon 2"_s, u"Offhand"_s},
        {u"Weapon 1 Swap"_s, u"Weapon 2"_s},
        {u"Weapon 2 Swap"_s, u"Offhand 2"_s},
        {u"Helmet"_s, u"Helmet"_s},
        {u"Body Armour"_s, u"Body Armour"_s},
        {u"Gloves"_s, u"Gloves"_s},
        {u"Boots"_s, u"Boots"_s},
        {u"Belt"_s, u"Belt"_s},
        {u"Amulet"_s, u"Amulet"_s},
        {u"Ring 1"_s, u"Ring 1"_s},
        {u"Ring 2"_s, u"Ring 2"_s},
        {u"Flask 1"_s, u"Life Flask"_s},
        {u"Flask 2"_s, u"Mana Flask"_s},
        {u"Charm 1"_s, u"Charm 1"_s},
        {u"Charm 2"_s, u"Charm 2"_s},
        {u"Charm 3"_s, u"Charm 3"_s},
    };
    return slots;
}

// The same slot in the other weapon set, or the slot itself when it isn't a
// weapon set's.
QString inOtherSet(const QString &slot)
{
    static const QHash<QString, QString> otherSet = {
        {u"Weapon"_s, u"Weapon 2"_s},
        {u"Offhand"_s, u"Offhand 2"_s},
        {u"Weapon 2"_s, u"Weapon"_s},
        {u"Offhand 2"_s, u"Offhand"_s},
    };
    return otherSet.value(slot, slot);
}

struct ItemSet
{
    int id = 0;
    bool swapInUse = false;               // its second weapon set is the one in use
    QList<std::pair<QString, int>> slots; // Path of Building's slot, the item's id
};

ItemSet readItemSet(QXmlStreamReader &reader)
{
    const QXmlStreamAttributes attributes = reader.attributes();
    ItemSet set;
    set.id = attributes.value(u"id").toInt();
    set.swapInUse = attributes.value(u"useSecondWeaponSet") == u"true";
    while (reader.readNextStartElement()) {
        if (reader.name() == u"Slot") {
            const QXmlStreamAttributes slot = reader.attributes();
            set.slots.append({slot.value(u"name").toString(), slot.value(u"itemId").toInt()});
        }
        reader.skipCurrentElement();
    }
    return set;
}

// The ids of the jewels in one of the build's passive trees, in the order
// of their nodes. A jewel stays put when you take back the socket's point,
// but then it doesn't count.
QList<int> readSpec(QXmlStreamReader &reader)
{
    const QXmlStreamAttributes attributes = reader.attributes();
    QSet<int> allocated;
    for (const QStringView node : attributes.value(u"nodes").split(u','))
        allocated.insert(node.toInt());

    QMap<int, int> jewels;
    while (reader.readNextStartElement()) {
        if (reader.name() != u"Sockets") {
            reader.skipCurrentElement();
            continue;
        }
        while (reader.readNextStartElement()) {
            const QXmlStreamAttributes socket = reader.attributes();
            if (const int node = socket.value(u"nodeId").toInt();
                reader.name() == u"Socket" && allocated.contains(node))
                jewels.insert(node, socket.value(u"itemId").toInt());
            reader.skipCurrentElement();
        }
    }
    return jewels.values();
}

// The numbers the build saved for its sidebar, by name: "Life", "FireResist"...
// The minions of its main skill have a list of their own, when it has some.
struct SavedStats
{
    QHash<QString, double> player;
    QHash<QString, double> minion;
};

SavedStats readSavedStats(QXmlStreamReader &reader)
{
    SavedStats stats;
    while (reader.readNextStartElement()) {
        QHash<QString, double> *list = nullptr;
        if (reader.name() == u"PlayerStat")
            list = &stats.player;
        else if (reader.name() == u"MinionStat")
            list = &stats.minion;
        if (list) {
            const QXmlStreamAttributes stat = reader.attributes();
            bool ok = false;
            if (const double value = stat.value(u"value").toDouble(&ok); ok && std::isfinite(value))
                list->insert(stat.value(u"stat").toString(), value);
        }
        reader.skipCurrentElement();
    }
    return stats;
}

} // namespace

std::optional<QString> decodeBuildCode(const QString &code)
{
    const QString trimmed = code.trimmed();
    if (trimmed.startsWith(u'<'))
        return trimmed;

    // Codes use the URL-safe alphabet, but arrive wrapped over lines or
    // converted to the standard one.
    QByteArray base64 = trimmed.toLatin1();
    base64.removeIf([](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; });
    base64.replace('+', '-').replace('/', '_');
    const auto decoded = QByteArray::fromBase64Encoding(base64, QByteArray::Base64UrlEncoding |
                                                                    QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.isEmpty())
        return std::nullopt;

    std::optional<QByteArray> xml = inflated(decoded.decoded, 15 + 32);
    if (!xml)
        xml = inflated(decoded.decoded, -15);
    if (!xml)
        return std::nullopt;
    return QString::fromUtf8(*xml);
}

QUrl buildCodeUrl(const QString &link)
{
    struct Site
    {
        QString host;
        QRegularExpression path; // with the build's id in the first group
        QString code;
    };
    const auto site = [](const QString &host, const QString &path, const QString &code) {
        return Site{host, QRegularExpression(u'^' + path + u"/?$"_s), code};
    };
    // Path of Building's list, also taking links to the code itself
    static const QList<Site> sites = {
        // pobb.in/AbC, or under its author: pobb.in/u/someone/AbC
        site(u"pobb.in"_s, uR"(/(?:u/[^/]+/|pob/)?([\w-]+)(?:/raw)?)"_s, u"https://pobb.in/pob/%1"_s),
        site(u"poe.ninja"_s, uR"(/(?:poe2/)?pob/(?:raw/)?([\w-]+))"_s,
             u"https://poe.ninja/poe2/pob/raw/%1"_s),
        site(u"poe2.ninja"_s, uR"(/pob/(?:raw/)?([\w-]+))"_s, u"https://poe.ninja/poe2/pob/raw/%1"_s),
        site(u"maxroll.gg"_s, uR"(/poe2/(?:api/)?pob/([\w-]+))"_s, u"https://maxroll.gg/poe2/api/pob/%1"_s),
        site(u"poe2db.tw"_s, uR"(/pob/([\w-]+)(?:/raw)?)"_s, u"https://poe2db.tw/pob/%1/raw"_s),
        site(u"pastebin.com"_s, uR"(/(?:raw/)?(\w+))"_s, u"https://pastebin.com/raw/%1"_s),
        site(u"pastebinp.com"_s, uR"(/(?:raw/)?(\w+))"_s, u"https://pastebinp.com/raw/%1"_s),
        site(u"rentry.co"_s, uR"(/(\w+))"_s, u"https://rentry.co/paste/%1/raw"_s),
    };

    const QUrl url = QUrl::fromUserInput(link.trimmed());
    if (url.scheme() != u"https" && url.scheme() != u"http")
        return {};
    QString host = url.host();
    if (host.startsWith(u"www."))
        host.remove(0, 4);
    for (const Site &s : sites) {
        if (s.host != host)
            continue;
        if (const auto m = s.path.match(url.path()); m.hasMatch())
            return QUrl(s.code.arg(m.captured(1)));
    }
    return {};
}

QString pobItemClass(const QString &slot, const QString &baseType)
{
    static const QHash<QString, QString> bySlot = {
        {u"Helmet"_s, u"Helmets"_s},
        {u"Body Armour"_s, u"Body Armours"_s},
        {u"Gloves"_s, u"Gloves"_s},
        {u"Boots"_s, u"Boots"_s},
        {u"Belt"_s, u"Belts"_s},
        {u"Amulet"_s, u"Amulets"_s},
        {u"Life Flask"_s, u"Life Flasks"_s},
        {u"Mana Flask"_s, u"Mana Flasks"_s},
    };
    if (const auto it = bySlot.constFind(slot); it != bySlot.cend())
        return *it;
    if (slot.startsWith(u"Ring"))
        return u"Rings"_s;
    if (slot.startsWith(u"Charm"))
        return u"Charms"_s;
    if (slot.startsWith(u"Jewel"))
        return u"Jewels"_s;
    if (const QString itemClass = weaponClass(baseType); !itemClass.isEmpty())
        return itemClass;
    return slot.startsWith(u"Offhand") ? u"Off-hands"_s : u"Weapons"_s;
}

PobItemText convertPobItem(const QString &raw, const QString &slot)
{
    static const QRegularExpression spec(uR"(^([A-Za-z ()]+):\s*(.*)$)"_s);
    static const QRegularExpression requirement(uR"(^Requires (Class|Level) (.+)$)"_s);
    static const QRegularExpression altVariant(
        uR"(^(Has|Selected) Alt Variant(?: (Two|Three|Four|Five))?$)"_s);
    static const QStringList altNumbers = {QString(), u"Two"_s, u"Three"_s, u"Four"_s, u"Five"_s};

    QStringList lines;
    for (const QString &line : raw.split(u'\n')) {
        if (const QString trimmed = line.trimmed(); !trimmed.isEmpty())
            lines << trimmed;
    }

    qsizetype i = 0;
    QString rarity = u"Rare"_s;
    if (const auto m = spec.match(lines.value(0)); m.hasMatch() && m.captured(1) == u"Rarity") {
        rarity = capitalized(m.captured(2).trimmed());
        i = 1;
    }
    // rare and unique items have a name above the base type
    const qsizetype nameLines = rarity == u"Rare" || rarity == u"Unique" ? 2 : 1;
    QStringList names;
    for (; i < lines.size() && names.size() < nameLines; ++i) {
        const QString &line = lines.at(i);
        if (line.contains(u':') || line.startsWith(u'{') || isState(line))
            break;
        names << line;
    }

    int itemLevel = 0;
    int levelReq = 0;
    int implicitLines = 0; // runes and enchants count as implicits here
    int variantCount = 0;
    int variant = 0;
    std::array<bool, 5> hasAlt{};
    std::array<int, 5> alt{};
    bool duplicateVariants = false;
    bool corrupted = false;
    QStringList properties;
    QList<PobMod> added; // runes and enchants
    QList<PobMod> implicits;
    QList<PobMod> explicits;

    for (; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        if (isState(line)) {
            corrupted = corrupted || line.endsWith(u"Corrupted");
            continue;
        }
        if (const auto m = requirement.match(line); m.hasMatch()) {
            if (m.captured(1) == u"Level")
                levelReq = m.captured(2).toInt();
            continue;
        }
        if (const auto m = spec.match(line); m.hasMatch()) {
            const QString name = m.captured(1).trimmed();
            const QString value = m.captured(2).trimmed();
            if (const QString property = propertyName(name); !property.isEmpty()) {
                properties << property + u": "_s + value;
                continue;
            }
            if (const auto a = altVariant.match(name); a.hasMatch()) {
                const auto n = altNumbers.indexOf(a.captured(2));
                if (a.captured(1) == u"Has")
                    hasAlt[n] = value == u"true";
                else
                    alt[n] = value.toInt();
                continue;
            }
            if (name == u"Item Level") {
                itemLevel = value.toInt();
                continue;
            }
            if (name == u"LevelReq" || name == u"Level") {
                levelReq = value.toInt();
                continue;
            }
            if (name == u"Implicits") {
                implicitLines = value.toInt();
                continue;
            }
            if (name == u"Variant") {
                ++variantCount;
                continue;
            }
            if (name == u"Selected Variant") {
                variant = value.toInt();
                continue;
            }
            if (name == u"Allow Duplicate Variants") {
                duplicateVariants = value == u"true";
                continue;
            }
            if (isMetadata(name))
                continue;
            // otherwise a mod with a colon in it, like "Grants Skill: ..."
        }

        PobMod mod = readMod(line);
        if (mod.tag == u"rune" || mod.tag == u"enchant") {
            added << mod;
        } else if (mod.tag == u"implicit" || added.size() + implicits.size() < implicitLines) {
            mod.tag = u"implicit"_s;
            implicits << mod;
        } else {
            explicits << mod;
        }
    }

    // Uniques with variants keep the mods of all of them. Without a choice
    // it's the last, the current version. Alternate variants pick more mods,
    // Mageblood's Legacies even the same one twice.
    if (variantCount > 0)
        variant = std::min(variant > 0 ? variant : variantCount, variantCount);
    QList<int> picked = {variant};
    for (std::size_t n = 0; n < alt.size(); ++n) {
        if (hasAlt[n])
            picked << alt[n];
    }
    const auto timesOn = [&](const PobMod &mod) -> qsizetype {
        if (mod.variants.isEmpty())
            return 1;
        const qsizetype times =
            std::count_if(picked.cbegin(), picked.cend(), [&](int v) { return mod.variants.contains(v); });
        return duplicateVariants ? times : std::min<qsizetype>(times, 1);
    };
    const auto section = [&](const QList<PobMod> &mods) {
        QStringList out;
        for (const PobMod &mod : mods) {
            const QString text = mod.tag.isEmpty() ? mod.text : mod.text + u" ("_s + mod.tag + u')';
            for (qsizetype n = timesOn(mod); n > 0; --n)
                out << text;
        }
        return out.join(u'\n');
    };

    const QString name = names.value(0, u"Unknown"_s);
    const QString base = names.value(1, name);
    const QString itemClass = pobItemClass(slot, base);
    QStringList header = {u"Item Class: "_s + itemClass, u"Rarity: "_s + rarity, name};
    if (base != name)
        header << base;

    QStringList blocks = {header.join(u'\n')};
    if (!properties.isEmpty())
        blocks << properties.join(u'\n');
    if (levelReq > 0)
        blocks << u"Requires: Level "_s + QString::number(levelReq);
    if (itemLevel > 0)
        blocks << u"Item Level: "_s + QString::number(itemLevel);
    for (const QString &mods : {section(added), section(implicits), section(explicits)}) {
        if (!mods.isEmpty())
            blocks << mods;
    }
    if (corrupted)
        blocks << u"Corrupted"_s;
    return {blocks.join(u"\n--------\n"_s), hasDamage(itemClass)};
}

std::optional<PobBuild> parsePobBuild(const QString &xml)
{
    QXmlStreamReader reader(xml);
    if (!reader.readNextStartElement() || reader.name() != u"PathOfBuilding2")
        return std::nullopt;

    PobBuild build;
    SavedStats stats;
    QHash<int, QString> items;
    QList<ItemSet> itemSets;
    int activeItemSet = 0;
    QList<QList<int>> trees;
    int activeTree = 0;
    while (reader.readNextStartElement()) {
        const QXmlStreamAttributes attributes = reader.attributes();
        if (reader.name() == u"Build") {
            build.level = attributes.value(u"level").toInt();
            build.className = attributes.value(u"className").toString();
            if (const QStringView ascendancy = attributes.value(u"ascendClassName"); ascendancy != u"None")
                build.ascendancy = ascendancy.toString();
            stats = readSavedStats(reader);
        } else if (reader.name() == u"Tree") {
            activeTree = attributes.value(u"activeSpec").toInt();
            while (reader.readNextStartElement()) {
                if (reader.name() == u"Spec")
                    trees << readSpec(reader);
                else
                    reader.skipCurrentElement();
            }
        } else if (reader.name() == u"Items") {
            activeItemSet = attributes.value(u"activeItemSet").toInt();
            while (reader.readNextStartElement()) {
                if (reader.name() == u"Item") {
                    const int id = reader.attributes().value(u"id").toInt();
                    // without the <ModRange> elements after the text
                    items.insert(id, reader.readElementText(QXmlStreamReader::SkipChildElements));
                } else if (reader.name() == u"ItemSet") {
                    itemSets << readItemSet(reader);
                } else {
                    reader.skipCurrentElement();
                }
            }
        } else {
            reader.skipCurrentElement();
        }
    }
    if (reader.hasError())
        return std::nullopt;

    // Path of Building falls back to the first item set, and to the last tree
    if (!itemSets.isEmpty()) {
        auto set = std::find_if(itemSets.cbegin(), itemSets.cend(),
                                [&](const ItemSet &s) { return s.id == activeItemSet; });
        if (set == itemSets.cend())
            set = itemSets.cbegin();
        build.swapInUse = set->swapInUse;
        for (const auto &[pobSlot, id] : set->slots) {
            const QString slot = gearSlot(pobSlot, set->swapInUse);
            if (const auto item = items.constFind(id); !slot.isEmpty() && item != items.cend())
                build.gear.insert(slot, convertPobItem(*item, slot));
        }
    }
    if (!trees.isEmpty()) {
        for (const int id : trees.at(std::clamp<qsizetype>(activeTree, 1, trees.size()) - 1)) {
            if (const auto item = items.constFind(id); item != items.cend())
                build.jewels << convertPobItem(*item, u"Jewel"_s);
        }
    }

    for (Element e : allElements) {
        const QString stat = elementName(e) + u"Resist"_s;
        if (const auto value = stats.player.constFind(stat); value != stats.player.cend())
            build.resists[e] = *value + stats.player.value(stat + u"OverCap"_s);
    }
    build.life = stats.player.value(u"Life"_s);
    build.energyShield = stats.player.value(u"EnergyShield"_s);
    // the combined damage, or the hits when it has none
    build.minionDps = std::max(stats.minion.value(u"CombinedDPS"_s), stats.minion.value(u"TotalDPS"_s));
    return build;
}

QString gearSlot(const QString &pobSlot, bool swapInUse)
{
    const QString slot = slotsByPobSlot().value(pobSlot);
    return swapInUse ? inOtherSet(slot) : slot;
}

QString pobSlot(const QString &slot, bool swapInUse)
{
    return slotsByPobSlot().key(swapInUse ? inOtherSet(slot) : slot);
}

QList<PobItem> pobWearing(const Equipped &gear, const QSet<QString> &fromPob, bool swapInUse)
{
    QList<PobItem> wearing;
    // in the order of our slots, not the hash's, which changes from run to run
    for (const QString &slot : allSlots()) {
        const auto item = gear.constFind(slot);
        const QString theirs = pobSlot(slot, swapInUse);
        if (item != gear.cend() && !fromPob.contains(slot) && !theirs.isEmpty())
            wearing << PobItem{.text = item->raw, .slot = theirs};
    }
    return wearing;
}

bool isPoe1Build(const QString &xml)
{
    QXmlStreamReader reader(xml);
    return reader.readNextStartElement() && reader.name() == u"PathOfBuilding";
}

} // namespace pou
