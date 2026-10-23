// SPDX-License-Identifier: MIT
#pragma once

#include "core/pobinstall.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

    // What its answers to evals have from now on, the way the bridge would
    // work them out, see eval in pobbridge.lua.
    bool answers(const QJsonObject &eval) const
    {
        QFile file(folder.filePath(u"eval.json"_s));
        return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(eval).toJson()) > 0;
    }
    // The items it was last asked to judge.
    QJsonArray asked() const
    {
        QFile file(folder.filePath(u"asked.json"_s));
        return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).array()
                                              : QJsonArray();
    }

    int count(const QString &name) const
    {
        QFile file(folder.filePath(name));
        return file.open(QIODevice::ReadOnly) ? file.readAll().toInt() : 0;
    }
};
