// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringView>
#include <QUrl>
#include <array>
#include <compare>
#include <optional>

namespace pou {

// A version as the release tags say it: v0.7.0 for a release, and
// v0.7.0-beta.4 for a beta leading up to it, which comes before it.
struct Version
{
    std::array<int, 3> numbers{};
    // Which beta of those numbers, or -1 for the release itself.
    int beta = -1;

    bool isBeta() const { return beta >= 0; }
    // "0.7.0" or "0.7.0-beta.4"
    QString text() const;

    friend bool operator==(const Version &, const Version &) = default;
    friend std::strong_ordering operator<=>(const Version &a, const Version &b);
};

// Reads "0.7.0", "v0.7.0" or "v0.7.0-beta.4". Anything else has no version.
std::optional<Version> parseVersion(QStringView text);

// The version this program was built as, which is a beta's too for the
// release builds of one. The packages say it with POU_BUILD_VERSION.
Version buildVersion();

// How the program was put on the computer, which decides how it can be
// updated.
enum class InstallKind {
    Source, // built by hand, or anything else
    Arch,
    Debian,
    Fedora,
    AppImage,
    Windows,
};

QString installKindId(InstallKind kind);
InstallKind installKindFromId(QStringView id);

struct ReleaseAsset
{
    QString name;
    QUrl url;
    qint64 size = 0;
};

struct Release
{
    QString tag; // v0.7.0-beta.4
    Version version;
    bool prerelease = false;
    QString notes; // GitHub's text, in Markdown
    QUrl page;     // the release on GitHub
    QList<ReleaseAsset> assets;

    bool isBeta() const { return prerelease || version.isBeta(); }
};

// Whether the address is a file or page of this project's releases on GitHub,
// the only place anything is downloaded from.
bool isReleaseUrl(const QUrl &url);

// The releases GitHub lists, as /repos/coreydeli/path-of-upgrading/releases
// gives them. Drafts and tags that aren't a version are left out, and so are
// pages and files that aren't this project's releases. Empty when it isn't
// what GitHub sends.
QList<Release> readReleases(const QByteArray &json);

// The newest release that is newer than the one you have. Betas count only
// when they're asked for, so a beta that turns them off waits for a release
// and never goes back to an older one.
std::optional<Release> newerRelease(const QList<Release> &releases, const Version &have, bool betas);

// The file to download for how it was installed, by the names the release
// workflow gives them, if the release has one.
std::optional<ReleaseAsset> assetFor(const Release &release, InstallKind kind);
// The release's list of checksums.
std::optional<ReleaseAsset> checksumsOf(const Release &release);

// File names and their SHA-256 sums, in lower case hex, from a SHA256SUMS
// file as sha256sum writes it.
QHash<QString, QString> parseChecksums(const QByteArray &text);

} // namespace pou

Q_DECLARE_METATYPE(pou::Release)
