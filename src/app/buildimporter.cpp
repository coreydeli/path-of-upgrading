// SPDX-License-Identifier: MIT
#include "app/buildimporter.h"

#include "app/characterlist.h"
#include "core/buildprofiles.h"
#include "core/datafiles.h"
#include "core/pobbuild.h"
#include "services/pobengine.h"

#include <QFileInfo>
#include <QVariantMap>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// far bigger than any build Path of Building saves
constexpr qint64 maxFileSize = 32 * 1024 * 1024;

QString imported(const BuildImport &done, const PobBuild &build)
{
    const QString &name = done.character.name;
    const QString what = buildName(build);
    QStringList said = {what.isEmpty() ? u"Imported the build for %1."_s.arg(name)
                                       : u"Imported the %1 build for %2."_s.arg(what, name)};
    if (done.items == 1)
        said << u"%1 now wears 1 item from it."_s.arg(name);
    else if (done.items > 1)
        said << u"%1 now wears %2 items from it."_s.arg(name, QString::number(done.items));
    else if (done.kept == 0)
        said << u"It has no items on."_s;
    if (done.kept == 1)
        said << u"Kept 1 item you copied in the game, which says more."_s;
    else if (done.kept > 1)
        said << u"Kept %1 items you copied in the game, which say more."_s.arg(done.kept);
    if (done.withoutDamage == 1)
        said << u"Copy your weapon in the game once, as the build doesn't say its damage."_s;
    else if (done.withoutDamage > 1)
        said << u"Copy your weapons in the game once, as the build doesn't say their damage."_s;
    if (done.profileChanged)
        said << u"Chose the %1 gear score profile for it."_s.arg(
            buildProfile(done.character.buildProfile)->name);
    return said.join(u' ');
}

} // namespace

BuildImporter::BuildImporter(CharacterList *characters, PobEngine *pob, QNetworkAccessManager *network,
                             QObject *parent)
    : QObject(parent)
    , m_characters(characters)
    , m_pob(pob)
    , m_fetcher(network)
{
    connect(&m_fetcher, &BuildFetcher::fetched, this, [this](const QString &code) {
        Q_EMIT busyChanged();
        if (const std::optional<QString> xml = decodeBuildCode(code))
            import(*xml, {});
        else
            finish(u"That link doesn't give a Path of Building code."_s, true);
    });
    connect(&m_fetcher, &BuildFetcher::failed, this, [this](const QString &why) {
        Q_EMIT busyChanged();
        finish(why, true);
    });
    connect(pob, &PobEngine::installChanged, this, &BuildImporter::refreshBuilds);
    refreshBuilds();
}

void BuildImporter::importText(const QString &text)
{
    cancel();
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        finish(u"Paste a build code or a link to one first."_s, true);
    } else if (m_fetcher.fetch(trimmed)) {
        finish({}, false);
        Q_EMIT busyChanged();
    } else if (const std::optional<QString> xml = decodeBuildCode(trimmed)) {
        import(*xml, {});
    } else {
        finish(u"That isn't a Path of Building code or a link to one."_s, true);
    }
}

void BuildImporter::importFile(const QString &path)
{
    cancel();
    const QFileInfo file(path);
    if (file.size() > maxFileSize) {
        finish(u"%1 is too big to be a build."_s.arg(file.fileName()), true);
        return;
    }
    QString why;
    const std::optional<QByteArray> data = readFile(path, &why);
    if (!data)
        finish(u"Couldn't read %1: %2"_s.arg(file.fileName(), why), true);
    else if (const std::optional<QString> xml = decodeBuildCode(QString::fromUtf8(*data)))
        import(*xml, file.completeBaseName());
    else
        finish(u"%1 isn't a Path of Building build."_s.arg(file.fileName()), true);
}

void BuildImporter::importFileUrl(const QUrl &file)
{
    if (file.isLocalFile())
        importFile(file.toLocalFile());
}

void BuildImporter::refreshBuilds()
{
    const std::optional<PobInstall> &install = m_pob->install();
    const QString folder = install ? pobBuildsFolder(*install) : QString();
    QList<SavedBuild> builds = folder.isEmpty() ? QList<SavedBuild>() : savedBuilds(folder);
    if (folder == m_folder && builds == m_builds)
        return;
    m_folder = folder;
    m_builds = std::move(builds);
    Q_EMIT buildsChanged();
}

QVariantList BuildImporter::builds() const
{
    QVariantList builds;
    for (const SavedBuild &build : m_builds)
        builds << QVariantMap{
            {u"name"_s, build.name}, {u"path"_s, build.path}, {u"modified"_s, build.modified}};
    return builds;
}

void BuildImporter::import(const QString &xml, const QString &nameIfNew)
{
    if (isPoe1Build(xml)) {
        finish(u"That's a build for Path of Exile 1."_s, true);
        return;
    }
    const std::optional<PobBuild> build = parsePobBuild(xml);
    if (!build) {
        finish(u"That isn't a Path of Exile 2 build."_s, true);
        return;
    }

    QString id = m_characters->current();
    if (!m_characters->find(id)) {
        QString name = nameIfNew.trimmed();
        if (name.isEmpty())
            name = build->ascendancy.isEmpty() ? build->className : build->ascendancy;
        id = m_characters->add(name.isEmpty() ? u"New character"_s : name);
        if (id.isEmpty()) {
            finish(m_characters->error(), true);
            return;
        }
    }
    if (const std::optional<BuildImport> done = m_characters->importBuild(id, xml, *build))
        finish(imported(*done, *build), false);
    else
        finish(m_characters->error(), true);
}

void BuildImporter::cancel()
{
    if (!m_fetcher.isFetching())
        return;
    m_fetcher.cancel();
    Q_EMIT busyChanged();
}

void BuildImporter::finish(const QString &message, bool failed)
{
    m_message = message;
    m_failed = failed;
    Q_EMIT messageChanged();
}

} // namespace pou
