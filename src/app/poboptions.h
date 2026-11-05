// SPDX-License-Identifier: MIT
#pragma once

#include "core/pobguide.h"
#include "core/pobinstall.h"
#include "core/settings.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtQmlIntegration/qqmlintegration.h>
#include <optional>

namespace pou {

class Opener;
class PobEngine;
class SettingsStore;

// The Path of Building settings as the settings window shows them, saved as
// soon as they're changed, with which one was found and what to do when none
// was.
class PobOptions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY changed)
    // Empty to look in the usual places.
    Q_PROPERTY(QString location READ location WRITE setLocation NOTIFY changed)
    Q_PROPERTY(double damageWeight READ damageWeight WRITE setDamageWeight NOTIFY changed)
    Q_PROPERTY(bool resistsFirst READ resistsFirst WRITE setResistsFirst NOTIFY changed)
    Q_PROPERTY(bool chaos READ chaos WRITE setChaos NOTIFY changed)
    Q_PROPERTY(bool keepDamage READ keepDamage WRITE setKeepDamage NOTIFY changed)
    Q_PROPERTY(bool keepDefence READ keepDefence WRITE setKeepDefence NOTIFY changed)
    Q_PROPERTY(bool spirit READ spirit WRITE setSpirit NOTIFY changed)
    Q_PROPERTY(bool attributes READ attributes WRITE setAttributes NOTIFY changed)

    // What the page says of where it is, see PobGuide.
    Q_PROPERTY(QString headline READ headline NOTIFY changed)
    Q_PROPERTY(QStringList details READ details NOTIFY changed)
    Q_PROPERTY(QStringList steps READ steps NOTIFY changed)
    Q_PROPERTY(QString installedAt READ installedAt NOTIFY changed)
    Q_PROPERTY(QString buildsAt READ buildsAt NOTIFY changed)
    Q_PROPERTY(bool offersFlatpak READ offersFlatpak NOTIFY changed)
    Q_PROPERTY(bool offersInstaller READ offersInstaller NOTIFY changed)
    Q_PROPERTY(QString folderNote READ folderNote NOTIFY changed)
    Q_PROPERTY(bool folderProblem READ folderProblem NOTIFY changed)
    Q_PROPERTY(QStringList places READ places NOTIFY changed)
    Q_PROPERTY(QString installCommand READ installCommand CONSTANT)

public:
    // They have to outlive it. The opener may be null, and then it opens and
    // copies nothing.
    PobOptions(SettingsStore *settings, PobEngine *pob, const PobPlaces &places, Opener *opener,
               QObject *parent = nullptr);

    bool isEnabled() const { return pob().enabled; }
    void setEnabled(bool enabled);
    QString location() const { return pob().location; }
    void setLocation(const QString &location);
    // A folder picked in a file dialog.
    Q_INVOKABLE void setLocationUrl(const QUrl &folder);
    // How much damage counts against defence, from 0 to 1.
    double damageWeight() const { return pob().damageWeight; }
    void setDamageWeight(double weight);
    bool resistsFirst() const { return pob().priorities.resistsFirst; }
    void setResistsFirst(bool first);
    bool chaos() const { return pob().priorities.chaos; }
    void setChaos(bool chaos);
    bool keepDamage() const { return pob().priorities.keepDamage; }
    void setKeepDamage(bool keep);
    bool keepDefence() const { return pob().priorities.keepDefence; }
    void setKeepDefence(bool keep);
    bool spirit() const { return pob().priorities.spirit; }
    void setSpirit(bool spirit);
    bool attributes() const { return pob().priorities.attributes; }
    void setAttributes(bool attributes);

    QString headline() const { return guide().headline; }
    QStringList details() const { return guide().details; }
    QStringList steps() const { return guide().steps; }
    QString installedAt() const { return guide().installedAt; }
    QString buildsAt() const { return guide().buildsAt; }
    bool offersFlatpak() const { return guide().getFlatpak; }
    bool offersInstaller() const { return guide().getInstaller; }
    QString folderNote() const { return guide().folderNote; }
    bool folderProblem() const { return guide().folderProblem; }
    QStringList places() const { return guide().places; }
    QString installCommand() const;

    // For the buttons, which open the pages where Path of Building can be
    // had, and copy the command that installs it.
    Q_INVOKABLE void openDiscover();
    Q_INVOKABLE void openFlathub();
    Q_INVOKABLE void openInstaller();
    Q_INVOKABLE void copyInstallCommand();
    // Clears the folder, so it looks in the usual places.
    Q_INVOKABLE void useUsualPlaces();

Q_SIGNALS:
    // For any of them, as when another Path of Building is found.
    void changed();

private:
    const PobSettings &pob() const;
    template<typename Edit>
    void change(Edit edit);
    // as it is until something changes
    const PobGuide &guide() const;
    void follow();

    SettingsStore *m_settings;
    PobEngine *m_pob;
    PobPlaces m_places;
    Opener *m_opener;
    mutable std::optional<PobGuide> m_guide;
};

} // namespace pou
