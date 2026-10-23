// SPDX-License-Identifier: MIT
#pragma once

#include "core/pobinstall.h"

#include <QFile>
#include <QTemporaryDir>

using namespace Qt::Literals::StringLiterals;

// A stand-in for Path of Building in a temporary folder, see fakepob.cpp.
struct FakePob
{
    QTemporaryDir folder;

    // with the fake's arguments for going wrong
    pou::PobInstall install(const QStringList &flags = {}) const
    {
        return {.folder = folder.path(), .program = QStringLiteral(FAKEPOB), .arguments = flags};
    }
    // how many times it has been started
    int runs() const { return count(u"runs"_s); }
    // and has stopped on its own
    int stops() const { return count(u"stops"_s); }

    int count(const QString &name) const
    {
        QFile file(folder.filePath(name));
        return file.open(QIODevice::ReadOnly) ? file.readAll().toInt() : 0;
    }
};
