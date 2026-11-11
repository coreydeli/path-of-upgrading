// SPDX-License-Identifier: MIT
#include "core/updates.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <limits>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

const QString releasesPath = u"/coreydeli/path-of-upgrading/releases/"_s;

// The release comes after all of its betas.
int betaRank(const Version &v)
{
    return v.beta < 0 ? std::numeric_limits<int>::max() : v.beta;
}

} // namespace

QString Version::text() const
{
    QString text = u"%1.%2.%3"_s.arg(numbers[0]).arg(numbers[1]).arg(numbers[2]);
    if (isBeta())
        text += u"-beta.%1"_s.arg(beta);
    return text;
}

std::strong_ordering operator<=>(const Version &a, const Version &b)
{
    if (const auto order = a.numbers <=> b.numbers; order != 0)
        return order;
    return betaRank(a) <=> betaRank(b);
}

std::optional<Version> parseVersion(QStringView text)
{
    static const QRegularExpression pattern(uR"(^v?(\d+)\.(\d+)\.(\d+)(?:-beta\.(\d+))?$)"_s);
    const QRegularExpressionMatch match = pattern.matchView(text);
    if (!match.hasMatch())
        return std::nullopt;
    Version version;
    for (int i = 0; i < 3; ++i) {
        bool ok = false;
        version.numbers[i] = match.capturedView(i + 1).toInt(&ok);
        if (!ok)
            return std::nullopt;
    }
    if (match.hasCaptured(4)) {
        bool ok = false;
        version.beta = match.capturedView(4).toInt(&ok);
        if (!ok)
            return std::nullopt;
    }
    return version;
}

Version buildVersion()
{
    return parseVersion(QStringView(u"" POU_BUILD_VERSION)).value_or(Version{});
}

QString installKindId(InstallKind kind)
{
    switch (kind) {
    case InstallKind::Arch:
        return u"arch"_s;
    case InstallKind::Debian:
        return u"deb"_s;
    case InstallKind::Fedora:
        return u"rpm"_s;
    case InstallKind::AppImage:
        return u"appimage"_s;
    case InstallKind::Windows:
        return u"windows"_s;
    case InstallKind::Source:
        break;
    }
    return u"source"_s;
}

InstallKind installKindFromId(QStringView id)
{
    for (InstallKind kind : {InstallKind::Arch, InstallKind::Debian, InstallKind::Fedora,
                             InstallKind::AppImage, InstallKind::Windows}) {
        if (id == installKindId(kind))
            return kind;
    }
    return InstallKind::Source;
}

bool isReleaseUrl(const QUrl &url)
{
    return url.isValid() && url.scheme() == u"https" && url.host() == u"github.com" && url.port() == -1 &&
           url.userInfo().isEmpty() && url.path().startsWith(releasesPath);
}

QList<Release> readReleases(const QByteArray &json)
{
    QList<Release> releases;
    const QJsonDocument document = QJsonDocument::fromJson(json);
    for (const QJsonValue &entry : document.array()) {
        const QJsonObject object = entry.toObject();
        if (object.value(u"draft"_s).toBool())
            continue;
        Release release;
        release.tag = object.value(u"tag_name"_s).toString();
        const std::optional<Version> version = parseVersion(release.tag);
        if (!version)
            continue;
        release.version = *version;
        release.prerelease = object.value(u"prerelease"_s).toBool();
        release.notes = object.value(u"body"_s).toString();
        const QUrl page(object.value(u"html_url"_s).toString());
        if (isReleaseUrl(page))
            release.page = page;
        for (const QJsonValue &file : object.value(u"assets"_s).toArray()) {
            const QJsonObject asset = file.toObject();
            const QUrl url(asset.value(u"browser_download_url"_s).toString());
            const QString name = asset.value(u"name"_s).toString();
            // the name ends up in a file path
            if (name.isEmpty() || name.contains(u'/') || name.contains(u'\\') || !isReleaseUrl(url))
                continue;
            release.assets.append({.name = name, .url = url, .size = asset.value(u"size"_s).toInteger()});
        }
        releases.append(release);
    }
    return releases;
}

std::optional<Release> newerRelease(const QList<Release> &releases, const Version &have, bool betas)
{
    std::optional<Release> newest;
    for (const Release &release : releases) {
        if (release.isBeta() && !betas)
            continue;
        if (release.version <= have)
            continue;
        if (!newest || release.version > newest->version)
            newest = release;
    }
    return newest;
}

std::optional<ReleaseAsset> assetFor(const Release &release, InstallKind kind)
{
    QString ending;
    switch (kind) {
    case InstallKind::Arch:
        ending = u".pkg.tar.zst"_s;
        break;
    case InstallKind::Debian:
        ending = u"_amd64.deb"_s;
        break;
    case InstallKind::Fedora:
        ending = u".x86_64.rpm"_s;
        break;
    case InstallKind::AppImage:
        ending = u"-x86_64.AppImage"_s;
        break;
    case InstallKind::Windows:
        ending = u"-setup.exe"_s;
        break;
    case InstallKind::Source:
        return std::nullopt;
    }
    const auto found = std::ranges::find_if(
        release.assets, [&](const ReleaseAsset &asset) { return asset.name.endsWith(ending); });
    if (found == release.assets.end())
        return std::nullopt;
    return *found;
}

std::optional<ReleaseAsset> checksumsOf(const Release &release)
{
    const auto found = std::ranges::find(release.assets, u"SHA256SUMS"_s, &ReleaseAsset::name);
    if (found == release.assets.end())
        return std::nullopt;
    return *found;
}

QHash<QString, QString> parseChecksums(const QByteArray &text)
{
    static const QRegularExpression line(uR"(^([0-9a-fA-F]{64}) [ *](.+)$)"_s);
    QHash<QString, QString> sums;
    for (const QString &row : QString::fromUtf8(text).split(u'\n', Qt::SkipEmptyParts)) {
        const QRegularExpressionMatch match = line.match(row.trimmed());
        if (match.hasMatch())
            sums.insert(match.captured(2), match.captured(1).toLower());
    }
    return sums;
}

} // namespace pou
