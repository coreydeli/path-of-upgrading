// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

namespace pou {

// Stat key -> value, in the order the stats were first added. Item texts list
// their mods in a meaningful order and several rankings fall back to it on
// ties, so a hash would lose information.
class StatMap
{
public:
    using Entry = std::pair<QString, double>;
    using const_iterator = std::vector<Entry>::const_iterator;

    StatMap() = default;
    StatMap(std::initializer_list<Entry> entries);

    bool isEmpty() const { return m_entries.empty(); }
    qsizetype size() const { return qsizetype(m_entries.size()); }
    bool contains(QStringView key) const { return find(key) != m_entries.end(); }

    std::optional<double> get(QStringView key) const;
    double value(QStringView key, double fallback = 0) const;

    // Replaces the value and keeps the key where it is.
    void set(const QString &key, double value);
    // Adds to the value, appending the key if it's new.
    void add(const QString &key, double value);
    bool remove(QStringView key);

    QStringList keys() const;

    const_iterator begin() const { return m_entries.begin(); }
    const_iterator end() const { return m_entries.end(); }

    friend bool operator==(const StatMap &, const StatMap &) = default;

private:
    std::vector<Entry>::const_iterator find(QStringView key) const;
    std::vector<Entry>::iterator find(QStringView key);

    std::vector<Entry> m_entries;
};

} // namespace pou
