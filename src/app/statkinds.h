// SPDX-License-Identifier: MIT
#pragma once

#include "core/statkind.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

// What a stat is for, as QML names it. The values are those of StatKind.
namespace ChipKind {
Q_NAMESPACE
QML_ELEMENT

enum Kind {
    Other = int(StatKind::Other),
    Resistance = int(StatKind::Resistance),
    Damage = int(StatKind::Damage),
    Defence = int(StatKind::Defence),
};
Q_ENUM_NS(Kind)

} // namespace ChipKind

// Lets the chips ask what kind of stat they show, as it's decided in C++.
class StatKinds : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    Q_INVOKABLE static pou::ChipKind::Kind of(const QString &text);
};

} // namespace pou
