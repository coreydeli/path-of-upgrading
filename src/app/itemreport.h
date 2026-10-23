// SPDX-License-Identifier: MIT
#pragma once

#include "app/itemjudge.h"

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
    // Where Path of Building is with it when it hasn't judged it: still
    // checking, why it couldn't, or that it could with a build.
    bool pobChecking = false;
    QString pobStatus;
    QStringList notes;
};

ItemReport reportOf(const ItemCheck &check);

} // namespace pou
