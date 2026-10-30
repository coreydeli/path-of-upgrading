// SPDX-License-Identifier: MIT
#pragma once

#include "core/resists.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>
#include <optional>

namespace pou {

class ActiveCharacter;
struct Character;

// The resistances of the character you play with what it wears now, as the
// gear page shows them: from its build in Path of Building, or typed in off
// the character sheet. Items are judged on whether they keep them capped.
class Resistances : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // Some of them are known.
    Q_PROPERTY(bool known READ isKnown NOTIFY changed)
    // They were typed in off the character sheet, so a build's don't replace
    // them. Turning it on starts from what's known, and off goes back to the
    // build's, or to none.
    Q_PROPERTY(bool fromSheet READ isFromSheet WRITE setFromSheet NOTIFY changed)
    // "Fire 75%, Cold 60%, Lightning 75%, Chaos -20%", of those known.
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    // Each as the character sheet has it, over the maximum too, 0 when it
    // isn't known. Setting one types them all in.
    Q_PROPERTY(double fire READ fire WRITE setFire NOTIFY changed)
    Q_PROPERTY(double cold READ cold WRITE setCold NOTIFY changed)
    Q_PROPERTY(double lightning READ lightning WRITE setLightning NOTIFY changed)
    Q_PROPERTY(double chaos READ chaos WRITE setChaos NOTIFY changed)
    // The most a resistance can be without what your gear adds to that, 75
    // unless the passive tree raises it.
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY changed)
    // How far over the maximum they should be, as some of what counts towards
    // them, like a charm, isn't always on.
    Q_PROPERTY(double margin READ margin WRITE setMargin NOTIFY changed)
    // Why the last change couldn't be made, empty when it could.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    // It has to outlive it.
    explicit Resistances(ActiveCharacter *active, QObject *parent = nullptr);

    bool isKnown() const { return m_live.has_value(); }
    bool isFromSheet() const;
    void setFromSheet(bool fromSheet);
    QString summary() const;

    double value(Element e) const;
    void setValue(Element e, double value);
    double fire() const { return value(Element::Fire); }
    void setFire(double v) { setValue(Element::Fire, v); }
    double cold() const { return value(Element::Cold); }
    void setCold(double v) { setValue(Element::Cold, v); }
    double lightning() const { return value(Element::Lightning); }
    void setLightning(double v) { setValue(Element::Lightning, v); }
    double chaos() const { return value(Element::Chaos); }
    void setChaos(double v) { setValue(Element::Chaos, v); }

    double maximum() const;
    void setMaximum(double maximum);
    double margin() const;
    void setMargin(double margin);

    QString error() const { return m_error; }

Q_SIGNALS:
    void changed();
    void errorChanged();

private:
    void refresh();
    // Saves them as read off the character sheet, with what you wear now.
    void typeIn(const PerElement<std::optional<double>> &values);
    void change(const std::function<void(Character &)> &edit);
    void setError(const QString &why);

    ActiveCharacter *m_active;
    std::optional<LiveResists> m_live;
    QString m_error;
};

} // namespace pou
