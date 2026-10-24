// SPDX-License-Identifier: MIT
#pragma once

#include "core/pobinstall.h"
#include "services/buildfetcher.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

class QNetworkAccessManager;

namespace pou {

class CharacterList;
class PobEngine;

// Imports Path of Building builds for the character you play, or for a new
// one when you play none: from a code, a link to one shared on pobb.in and
// the like, or a file Path of Building saved.
class BuildImporter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // Downloading the build behind a link.
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    // What came of the last import, for people, empty while there's none.
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool failed READ hasFailed NOTIFY messageChanged)
    // The builds saved in Path of Building, last saved first, each with its
    // "name", "path" and when it was "modified".
    Q_PROPERTY(QVariantList builds READ builds NOTIFY buildsChanged)
    // Where they are, empty without Path of Building.
    Q_PROPERTY(QString buildsFolder READ buildsFolder NOTIFY buildsChanged)

public:
    // They have to outlive it.
    BuildImporter(CharacterList *characters, PobEngine *pob, QNetworkAccessManager *network,
                  QObject *parent = nullptr);

    // A code, the XML, or a link to a build. In place of one still
    // downloading.
    Q_INVOKABLE void importText(const QString &text);
    Q_INVOKABLE void importFile(const QString &path);
    Q_INVOKABLE void importFileUrl(const QUrl &file);
    // Looks in Path of Building's folder again, as for a build you've saved
    // there since.
    Q_INVOKABLE void refreshBuilds();

    bool isBusy() const { return m_fetcher.isFetching(); }
    QString message() const { return m_message; }
    bool hasFailed() const { return m_failed; }
    QVariantList builds() const;
    QString buildsFolder() const { return m_folder; }

Q_SIGNALS:
    void busyChanged();
    void messageChanged();
    void buildsChanged();

private:
    // A new character for it is named nameIfNew, or after the build.
    void import(const QString &xml, const QString &nameIfNew);
    void cancel();
    void finish(const QString &message, bool failed);

    CharacterList *m_characters;
    PobEngine *m_pob;
    BuildFetcher m_fetcher;
    QString m_message;
    bool m_failed = false;
    QString m_folder;
    QList<SavedBuild> m_builds;
};

} // namespace pou
