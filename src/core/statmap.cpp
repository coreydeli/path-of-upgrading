// SPDX-License-Identifier: MIT
#include "core/statmap.h"

#include <algorithm>

namespace pou {

StatMap::StatMap(std::initializer_list<Entry> entries)
{
    for (const Entry &e : entries)
        set(e.first, e.second);
}

std::optional<double> StatMap::get(QStringView key) const
{
    const auto it = find(key);
    if (it == m_entries.end())
        return std::nullopt;
    return it->second;
}

double StatMap::value(QStringView key, double fallback) const
{
    return get(key).value_or(fallback);
}

void StatMap::set(const QString &key, double value)
{
    if (const auto it = find(key); it != m_entries.end())
        it->second = value;
    else
        m_entries.emplace_back(key, value);
}

void StatMap::add(const QString &key, double value)
{
    if (const auto it = find(key); it != m_entries.end())
        it->second += value;
    else
        m_entries.emplace_back(key, value);
}

bool StatMap::remove(QStringView key)
{
    const auto it = find(key);
    if (it == m_entries.end())
        return false;
    m_entries.erase(it);
    return true;
}

QStringList StatMap::keys() const
{
    QStringList out;
    out.reserve(size());
    for (const Entry &e : m_entries)
        out << e.first;
    return out;
}

std::vector<StatMap::Entry>::const_iterator StatMap::find(QStringView key) const
{
    return std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry &e) { return e.first == key; });
}

std::vector<StatMap::Entry>::iterator StatMap::find(QStringView key)
{
    return std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry &e) { return e.first == key; });
}

} // namespace pou
