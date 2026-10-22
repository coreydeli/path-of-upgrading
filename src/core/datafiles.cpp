// SPDX-License-Identifier: MIT
#include "core/datafiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

void setError(QString *error, const QString &why)
{
    if (error)
        *error = why;
}

} // namespace

std::optional<QByteArray> readFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return std::nullopt;
    }
    return file.readAll();
}

bool writeFile(const QString &path, const QByteArray &data, QString *error)
{
    const QString folder = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(folder)) {
        setError(error, u"can't make the folder %1"_s.arg(folder));
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, file.errorString());
        return false;
    }
    file.write(data);
    if (!file.commit()) {
        setError(error, file.errorString());
        return false;
    }
    return true;
}

std::optional<QJsonObject> readJsonFile(const QString &path, QString *error)
{
    const std::optional<QByteArray> data = readFile(path, error);
    if (!data)
        return std::nullopt;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(*data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, parseError.errorString());
        return std::nullopt;
    }
    if (!doc.isObject()) {
        setError(error, u"not a JSON object"_s);
        return std::nullopt;
    }
    return doc.object();
}

bool writeJsonFile(const QString &path, const QJsonObject &object, QString *error)
{
    return writeFile(path, QJsonDocument(object).toJson(), error);
}

} // namespace pou
