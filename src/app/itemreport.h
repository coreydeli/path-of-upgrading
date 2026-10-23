// SPDX-License-Identifier: MIT
#pragma once

#include "app/itemjudge.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

// How a part of a report reads, which its colour shows.
namespace Tone {
Q_NAMESPACE
QML_ELEMENT

enum Tone {
    Plain,
    Good,
    Bad,
    Warning, // good, with a catch
};
Q_ENUM_NS(Tone)

} // namespace Tone

// A number the item changes.
struct ReportFigure
{
    Q_GADGET
    QML_VALUE_TYPE(reportFigure)

    Q_PROPERTY(QString label MEMBER label)
    Q_PROPERTY(QString change MEMBER change)
    Q_PROPERTY(QString from MEMBER from)
    Q_PROPERTY(QString to MEMBER to)
    Q_PROPERTY(QString note MEMBER note)
    Q_PROPERTY(pou::Tone::Tone tone MEMBER tone)
    Q_PROPERTY(bool minor MEMBER minor)

public:
    QString label;  // "Life"
    QString change; // "+150", "+4.2%"
    // Empty when only the change is known.
    QString from;
    QString to;
    QString note; // "under the cap"
    Tone::Tone tone = Tone::Plain;
    bool minor = false; // one to leave out when there's little room

    friend bool operator==(const ReportFigure &, const ReportFigure &) = default;
};

// What the popup says about an item you checked.
struct ItemReport
{
    Q_GADGET
    QML_VALUE_TYPE(itemReport)

    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString rarity MEMBER rarity)
    Q_PROPERTY(QStringList details MEMBER details)
    Q_PROPERTY(QString verdict MEMBER verdict)
    Q_PROPERTY(pou::Tone::Tone verdictTone MEMBER verdictTone)
    Q_PROPERTY(QString score MEMBER score)
    Q_PROPERTY(QString scoreLabel MEMBER scoreLabel)
    Q_PROPERTY(QString versus MEMBER versus)
    Q_PROPERTY(QString gearScore MEMBER gearScore)
    Q_PROPERTY(QString gearGrade MEMBER gearGrade)
    Q_PROPERTY(QString yourGearScore MEMBER yourGearScore)
    Q_PROPERTY(QString gearBuild MEMBER gearBuild)
    Q_PROPERTY(QStringList losses MEMBER losses)
    Q_PROPERTY(QStringList risks MEMBER risks)
    Q_PROPERTY(QString caps MEMBER caps)
    Q_PROPERTY(QList<pou::ReportFigure> headline MEMBER headline)
    Q_PROPERTY(QList<pou::ReportFigure> changes MEMBER changes)
    Q_PROPERTY(bool pobChecking MEMBER pobChecking)
    Q_PROPERTY(QString pobStatus MEMBER pobStatus)
    Q_PROPERTY(QStringList notes MEMBER notes)

public:
    QString name;
    QString rarity;      // as the game says it, "Rare"
    QStringList details; // its base type, item class and item level
    QString verdict;     // "Upgrade", empty for an item that isn't gear
    Tone::Tone verdictTone = Tone::Plain;
    // How much better it is, "+4%", and by what, "for your build". Empty when
    // there's nothing to compare it with.
    QString score;
    QString scoreLabel;
    QString versus; // "vs Grim Crown"
    // Its gear score, "78", and grade, "A", next to yours in the slot. Empty
    // when it has none, or Path of Building has judged it.
    QString gearScore;
    QString gearGrade;
    QString yourGearScore;
    QString gearBuild; // the build profile's name
    // Resistance or damage it loses, which matters more than its risks.
    QStringList losses;
    QStringList risks;
    QString caps; // "Caps your fire and cold resistances"
    // What Path of Building says it does to your damage, effective HP and
    // resistances.
    QList<ReportFigure> headline;
    // What equipping it changes on your character, resistances first.
    QList<ReportFigure> changes;
    // Where Path of Building is with it when it hasn't judged it: still
    // checking, why it couldn't, or that it could with a build.
    bool pobChecking = false;
    QString pobStatus;
    QStringList notes;
};

ItemReport reportOf(const ItemCheck &check);

} // namespace pou
