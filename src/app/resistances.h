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
// gear page shows them: from its build in Path of Building, or typed in from
// the game's character sheet. Items are judged on whether they keep them capped.
class Resistances : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // Some of them are known.
    Q_PROPERTY(bool known READ isKnown NOTIFY changed)
    // They were typed in from the character sheet, so a build's don't replace
    // them. Turning it on starts from what's known, and off goes back to the
    // build's, or to none.
    Q_PROPERTY(bool fromSheet READ isFromSheet WRITE setFromSheet NOTIFY changed)
    // Where they come from, as a plain line: the build, what you typed in, or
    // that they aren't known yet.
    Q_PROPERTY(QString sourceNote READ sourceNote NOTIFY changed)
    // "Fire 75%, Cold 60%, Lightning 75%, Chaos -20%", of those known.
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    // Each as the character sheet has it, over the maximum too, 0 when it
    // isn't known. Setting one types them all in.
    Q_PROPERTY(double fire READ fire WRITE setFire NOTIFY changed)
    Q_PROPERTY(double cold READ cold WRITE setCold NOTIFY changed)
    Q_PROPERTY(double lightning READ lightning WRITE setLightning NOTIFY changed)
    Q_PROPERTY(double chaos READ chaos WRITE setChaos NOTIFY changed)
    // The most each can be without what your gear adds to that, 75 unless the
    // passive tree raises it. A build's are taken when it gives them, until
    // you set one yourself.
    Q_PROPERTY(double fireMaximum READ fireMaximum WRITE setFireMaximum NOTIFY changed)
    Q_PROPERTY(double coldMaximum READ coldMaximum WRITE setColdMaximum NOTIFY changed)
    Q_PROPERTY(double lightningMaximum READ lightningMaximum WRITE setLightningMaximum NOTIFY changed)
    Q_PROPERTY(double chaosMaximum READ chaosMaximum WRITE setChaosMaximum NOTIFY changed)
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
    QString sourceNote() const;

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

    double maximum(Element e) const;
    void setMaximum(Element e, double maximum);
    double fireMaximum() const { return maximum(Element::Fire); }
    void setFireMaximum(double v) { setMaximum(Element::Fire, v); }
    double coldMaximum() const { return maximum(Element::Cold); }
    void setColdMaximum(double v) { setMaximum(Element::Cold, v); }
    double lightningMaximum() const { return maximum(Element::Lightning); }
    void setLightningMaximum(double v) { setMaximum(Element::Lightning, v); }
    double chaosMaximum() const { return maximum(Element::Chaos); }
    void setChaosMaximum(double v) { setMaximum(Element::Chaos, v); }
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
