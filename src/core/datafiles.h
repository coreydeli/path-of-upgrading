// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <optional>

namespace pou {

// The files the app keeps your characters and its caches in.

// Empty when the file is missing or can't be read, with why in error when
// it's given.
std::optional<QByteArray> readFile(const QString &path, QString *error = nullptr);

// Replaces the file in one go, so a crash on the way leaves the old one as
// it was, and makes its folder when there's none. False when it can't, with
// why in error when it's given.
bool writeFile(const QString &path, const QByteArray &data, QString *error = nullptr);

// The same for a file that holds a JSON object, which reading also checks.
std::optional<QJsonObject> readJsonFile(const QString &path, QString *error = nullptr);
bool writeJsonFile(const QString &path, const QJsonObject &object, QString *error = nullptr);

} // namespace pou
