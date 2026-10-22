// SPDX-License-Identifier: MIT
#pragma once

#include <QJsonObject>
#include <QString>
#include <optional>

namespace pou {

// The JSON files the app keeps your characters and its caches in.

// The object in the file. Empty when the file is missing, can't be read or
// doesn't hold an object, with why in error when it's given.
std::optional<QJsonObject> readJsonFile(const QString &path, QString *error = nullptr);

// Replaces the file in one go, so a crash on the way leaves the old one as
// it was, and makes its folder when there's none. False when it can't, with
// why in error when it's given.
bool writeJsonFile(const QString &path, const QJsonObject &object, QString *error = nullptr);

} // namespace pou
