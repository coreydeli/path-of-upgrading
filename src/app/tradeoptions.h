// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class RatesStore;
class SettingsStore;
class TradeDataStore;

// The trade settings as the settings window shows them, saved as soon as
// they're changed. The sellers and the currency go by the trade site's ids
// for them, "available" and "divine".
class TradeOptions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY changed)
    // One of the leagues, or empty to follow the current one.
    Q_PROPERTY(QString league READ league WRITE setLeague NOTIFY changed)
    Q_PROPERTY(bool hardcore READ isHardcore WRITE setHardcore NOTIFY changed)
    Q_PROPERTY(bool soloSelfFound READ isSoloSelfFound WRITE setSoloSelfFound NOTIFY changed)
    Q_PROPERTY(QString sellers READ sellers WRITE setSellers NOTIFY changed)
    Q_PROPERTY(double budgetMin READ budgetMin WRITE setBudgetMin NOTIFY changed)
    Q_PROPERTY(double budgetMax READ budgetMax WRITE setBudgetMax NOTIFY changed)
    Q_PROPERTY(QString currency READ currency WRITE setCurrency NOTIFY changed)
    Q_PROPERTY(double divineRate READ divineRate WRITE setDivineRate NOTIFY changed)
    Q_PROPERTY(bool autoRate READ isAutoRate WRITE setAutoRate NOTIFY changed)

    Q_PROPERTY(QStringList leagues READ leagues NOTIFY changed)
    Q_PROPERTY(QString searchedLeague READ searchedLeague NOTIFY changed)
    Q_PROPERTY(QString unavailable READ unavailable NOTIFY changed)
    Q_PROPERTY(double ninjaRate READ ninjaRate NOTIFY changed)

public:
    // They have to outlive it.
    TradeOptions(SettingsStore *settings, TradeDataStore *data, RatesStore *rates, QObject *parent = nullptr);

    bool isEnabled() const { return trade().enabled; }
    void setEnabled(bool enabled);
    QString league() const { return trade().league.name; }
    void setLeague(const QString &league);
    bool isHardcore() const { return trade().league.hardcore; }
    void setHardcore(bool hardcore);
    bool isSoloSelfFound() const { return trade().league.soloSelfFound; }
    void setSoloSelfFound(bool soloSelfFound);
    QString sellers() const;
    void setSellers(const QString &sellers);
    // Either end left open with 0.
    double budgetMin() const { return trade().budget.min; }
    void setBudgetMin(double min);
    double budgetMax() const { return trade().budget.max; }
    void setBudgetMax(double max);
    QString currency() const;
    void setCurrency(const QString &currency);
    // Exalted Orbs to a Divine Orb, as you typed it.
    double divineRate() const { return trade().budget.divineRate; }
    void setDivineRate(double rate);
    // Whether poe.ninja's rate goes before yours.
    bool isAutoRate() const { return trade().autoRate; }
    void setAutoRate(bool autoRate);

    // The site's leagues, without their hardcore versions, and yours when
    // it isn't one of them, as before the site has said. Standard is always
    // there.
    QStringList leagues() const;
    // Where searches go: "Hardcore Runes of Aldur".
    QString searchedLeague() const;
    // Why there are no trade searches, or empty when there are.
    QString unavailable() const;
    // Exalted Orbs to a Divine Orb in the searched league, from poe.ninja, or
    // 0 while it hasn't said.
    double ninjaRate() const;

Q_SIGNALS:
    // For any of them, as when the site's leagues or poe.ninja's rates come
    // in.
    void changed();

private:
    const TradeSettings &trade() const;
    League searched() const;
    template<typename Edit>
    void change(Edit edit);

    SettingsStore *m_settings;
    TradeDataStore *m_data;
    RatesStore *m_rates;
};

} // namespace pou
