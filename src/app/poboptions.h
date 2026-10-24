// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class PobEngine;
class SettingsStore;

// The Path of Building settings as the settings window shows them, saved as
// soon as they're changed, with which one was found.
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
    Q_PROPERTY(bool spirit READ spirit WRITE setSpirit NOTIFY changed)
    Q_PROPERTY(bool attributes READ attributes WRITE setAttributes NOTIFY changed)

    Q_PROPERTY(QString found READ found NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)

public:
    // They have to outlive it.
    PobOptions(SettingsStore *settings, PobEngine *pob, QObject *parent = nullptr);

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
    bool spirit() const { return pob().priorities.spirit; }
    void setSpirit(bool spirit);
    bool attributes() const { return pob().priorities.attributes; }
    void setAttributes(bool attributes);

    // Which one it uses, as "Using the Path of Building Flatpak.", or empty
    // when there's none.
    QString found() const;
    // Why there's none, or why it couldn't start, or empty.
    QString problem() const;

Q_SIGNALS:
    // For any of them, as when another Path of Building is found.
    void changed();

private:
    const PobSettings &pob() const;
    template<typename Edit>
    void change(Edit edit);

    SettingsStore *m_settings;
    PobEngine *m_pob;
};

} // namespace pou
