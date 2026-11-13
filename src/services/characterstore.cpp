// SPDX-License-Identifier: MIT
#include "services/characterstore.h"

#include "core/datafiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {

CharacterStore::CharacterStore(const QString &folder)
    : m_folder(folder)
{}

void CharacterStore::load()
{
    m_characters.clear();
    m_problems.clear();
    const QFileInfoList files = QDir(m_folder).entryInfoList({u"*.json"_s}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : files) {
        QString why;
        const std::optional<QJsonObject> json = readJsonFile(file.filePath(), &why);
        std::optional<Character> character = json ? characterFromJson(*json) : std::nullopt;
        if (json && !character)
            why = u"saved by a newer version of Path of Upgrading"_s;
        if (character)
            m_characters.insert(file.completeBaseName(), std::move(*character));
        else
            m_problems << u"%1: %2"_s.arg(file.fileName(), why);
    }
}

QStringList CharacterStore::ids() const
{
    QStringList ids = m_characters.keys();
    std::sort(ids.begin(), ids.end(), [this](const QString &a, const QString &b) {
        const int byName =
            m_characters.constFind(a)->name.compare(m_characters.constFind(b)->name, Qt::CaseInsensitive);
        return byName != 0 ? byName < 0 : a < b;
    });
    return ids;
}

const Character *CharacterStore::find(const QString &id) const
{
    const auto it = m_characters.constFind(id);
    return it == m_characters.cend() ? nullptr : &*it;
}

std::optional<QString> CharacterStore::add(const Character &character)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!writeJsonFile(pathFor(id, u".json"), toJson(character), &m_error))
        return std::nullopt;
    m_characters.insert(id, character);
    return id;
}

bool CharacterStore::save(const QString &id, const Character &character)
{
    if (!m_characters.contains(id))
        return fail(u"there's no character %1"_s.arg(id));
    if (!writeJsonFile(pathFor(id, u".json"), toJson(character), &m_error))
        return false;
    m_characters.insert(id, character);
    return true;
}

bool CharacterStore::remove(const QString &id)
{
    if (!m_characters.contains(id))
        return fail(u"there's no character %1"_s.arg(id));
    // the build goes first, so a failure leaves a character without its build
    // rather than a build without its character
    for (QStringView suffix : {u".xml", u".json"}) {
        QFile file(pathFor(id, suffix));
        if (file.exists() && !file.remove())
            return fail(file.errorString());
    }
    m_characters.remove(id);
    return true;
}

std::optional<QString> CharacterStore::pobBuild(const QString &id) const
{
    if (!m_characters.contains(id))
        return std::nullopt;
    const std::optional<QByteArray> xml = readFile(pathFor(id, u".xml"));
    if (!xml)
        return std::nullopt;
    return QString::fromUtf8(*xml);
}

bool CharacterStore::savePobBuild(const QString &id, const QString &xml)
{
    if (!m_characters.contains(id))
        return fail(u"there's no character %1"_s.arg(id));
    return writeFile(pathFor(id, u".xml"), xml.toUtf8(), &m_error);
}

QString CharacterStore::pathFor(const QString &id, QStringView suffix) const
{
    return m_folder + u'/' + id + suffix;
}

bool CharacterStore::fail(const QString &why)
{
    m_error = why;
    return false;
}

} // namespace pou
