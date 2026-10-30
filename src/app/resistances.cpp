// SPDX-License-Identifier: MIT
#include "app/resistances.h"

#include "app/activecharacter.h"
#include "core/buildimport.h"
#include "core/numbers.h"

#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {

Resistances::Resistances(ActiveCharacter *active, QObject *parent)
    : QObject(parent)
    , m_active(active)
{
    connect(active, &ActiveCharacter::changed, this, &Resistances::refresh);
    refresh();
}

bool Resistances::isFromSheet() const
{
    const std::optional<ResistRecord> &resists = m_active->character().resists;
    return resists && resists->source == ResistSource::Game;
}

void Resistances::setFromSheet(bool fromSheet)
{
    if (fromSheet == isFromSheet() || !m_active->exists())
        return;
    if (fromSheet) {
        typeIn(m_live ? m_live->values : PerElement<std::optional<double>>());
        return;
    }
    const std::optional<PobBuild> &build = m_active->build();
    change([&build](Character &c) {
        c.resists = build ? buildResists(*build, c.resists ? &*c.resists : nullptr) : std::nullopt;
    });
}

QString Resistances::summary() const
{
    QStringList parts;
    for (Element e : allElements) {
        if (m_live && m_live->values[e])
            parts << u"%1 %2%"_s.arg(elementName(e), formatNumber(*m_live->values[e]));
    }
    return parts.join(u", ");
}

double Resistances::value(Element e) const
{
    return m_live ? m_live->values[e].value_or(0) : 0;
}

void Resistances::setValue(Element e, double value)
{
    if (!std::isfinite(value) || (isFromSheet() && m_live && m_live->values[e] == value))
        return;
    PerElement<std::optional<double>> values = m_live ? m_live->values : PerElement<std::optional<double>>();
    values[e] = value;
    typeIn(values);
}

double Resistances::maximum() const
{
    return m_live ? m_live->cap : 75;
}

void Resistances::setMaximum(double maximum)
{
    if (!m_live || !std::isfinite(maximum) || maximum <= 0 || maximum == this->maximum())
        return;
    change([maximum](Character &c) {
        if (c.resists)
            c.resists->cap = maximum;
    });
}

double Resistances::margin() const
{
    return m_live ? m_live->buffer : 0;
}

void Resistances::setMargin(double margin)
{
    if (!m_live || !std::isfinite(margin) || margin < 0 || margin == this->margin())
        return;
    change([margin](Character &c) {
        if (c.resists) {
            c.resists->buffer = margin;
            c.resists->bufferSet = true;
        }
    });
}

void Resistances::refresh()
{
    const Character &c = m_active->character();
    m_live = c.resists ? liveResists(*c.resists, c.gear) : std::nullopt;
    Q_EMIT changed();
}

void Resistances::typeIn(const PerElement<std::optional<double>> &values)
{
    ResistEntry entry;
    for (Element e : allElements)
        entry.values[e] = values[e].value_or(0);
    change([&entry](Character &c) {
        c.resists = snapshotResists(entry, c.gear, ResistSource::Game, c.resists ? &*c.resists : nullptr);
    });
}

void Resistances::change(const std::function<void(Character &)> &edit)
{
    setError(m_active->change(edit) ? QString() : m_active->errorString());
}

void Resistances::setError(const QString &why)
{
    if (why == m_error)
        return;
    m_error = why;
    Q_EMIT errorChanged();
}

} // namespace pou
