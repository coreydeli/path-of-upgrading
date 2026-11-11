// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

// What GitHub's list of releases says, for made-up releases.

inline const QString releaseBase = QStringLiteral("https://github.com/coreydeli/path-of-upgrading/releases");

inline QJsonObject fakeAsset(const QString &tag, const QString &name)
{
    const QString url =
        QString(releaseBase + QStringLiteral("/download/") + tag + QStringLiteral("/") + name);
    QJsonObject asset;
    asset[QStringLiteral("name")] = name;
    asset[QStringLiteral("size")] = 1234;
    asset[QStringLiteral("browser_download_url")] = url;
    return asset;
}

// A release with the files the workflow makes for it, like 0.7.0 or 0.7.0-beta.4.
inline QJsonObject fakeRelease(const QString &version, bool prerelease = false, bool draft = false)
{
    const QString tag = QStringLiteral("v") + version;
    QString plain = version;
    plain.replace(QStringLiteral("-beta."), QStringLiteral("beta"));
    QString dotted = version;
    dotted.replace(QStringLiteral("-beta."), QStringLiteral(".beta."));
    QJsonArray assets;
    for (const QString &name : {
             QStringLiteral("PathOfUpgrading-%1-x86_64.AppImage").arg(version),
             QStringLiteral("path-of-upgrading-%1-setup.exe").arg(version),
             QStringLiteral("path-of-upgrading-%1-windows.zip").arg(version),
             QStringLiteral("path-of-upgrading-%1.tar.gz").arg(version),
             QStringLiteral("path-of-upgrading-%1-1-x86_64.pkg.tar.zst").arg(plain),
             QStringLiteral("path-of-upgrading_%1-1_amd64.deb").arg(dotted),
             QStringLiteral("path-of-upgrading-%1-1.fc45.x86_64.rpm").arg(dotted),
             QStringLiteral("SHA256SUMS"),
         })
        assets.append(fakeAsset(tag, name));
    QJsonObject release;
    release[QStringLiteral("tag_name")] = tag;
    release[QStringLiteral("prerelease")] = prerelease;
    release[QStringLiteral("draft")] = draft;
    release[QStringLiteral("body")] = QStringLiteral("What is new in %1.").arg(version);
    release[QStringLiteral("html_url")] = QString(releaseBase + QStringLiteral("/tag/") + tag);
    release[QStringLiteral("assets")] = assets;
    return release;
}

inline QByteArray fakeReleases(const QList<QJsonObject> &releases)
{
    QJsonArray list;
    for (const QJsonObject &release : releases)
        list.append(release);
    return QJsonDocument(list).toJson(QJsonDocument::Compact);
}
