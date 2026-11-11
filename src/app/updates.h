// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>

namespace pou {

class Opener;
class SettingsStore;
class UpdateChecker;

// What the window says about updates: whether to look for them, whether betas
// count, the result of the last look in words, and the newer release when
// there is one, with the choices of what to do about it. The settings are
// saved as soon as they change.
class Updates : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    Q_PROPERTY(bool checkOnLaunch READ checksOnLaunch WRITE setCheckOnLaunch NOTIFY changed)
    Q_PROPERTY(bool includeBetas READ includesBetas WRITE setIncludeBetas NOTIFY changed)
    // "0.7.0" or "0.7.0-beta.4", the one you have
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool checking READ isChecking NOTIFY changed)
    // What the last look found, or why it didn't, in words. Empty before the
    // first look.
    Q_PROPERTY(QString status READ status NOTIFY changed)
    // Whether there's a release to offer, which you haven't skipped or put off
    Q_PROPERTY(bool pending READ isPending NOTIFY changed)
    Q_PROPERTY(QString newVersion READ newVersion NOTIFY changed)
    // What's new in it, as plain text
    Q_PROPERTY(QString notes READ notes NOTIFY changed)

public:
    // They have to outlive it. The opener may be null, and then it opens nothing.
    Updates(UpdateChecker *checker, SettingsStore *settings, Opener *opener, QObject *parent = nullptr);

    bool checksOnLaunch() const;
    void setCheckOnLaunch(bool check);
    bool includesBetas() const;
    void setIncludeBetas(bool include);
    QString version() const;
    bool isChecking() const;
    QString status() const;
    bool isPending() const;
    QString newVersion() const;
    QString notes() const;

    // Looks now, and offers the release again if it was skipped or put off.
    Q_INVOKABLE void checkNow();
    // Takes the newer release: opens its page on GitHub.
    Q_INVOKABLE void updateNow();
    // Leaves it for the next time the program starts.
    Q_INVOKABLE void later();
    // Doesn't offer this version again. A newer one is offered.
    Q_INVOKABLE void skip();

Q_SIGNALS:
    void changed();
    // A look found a release to offer, which the window shows or, with the
    // window hidden, the desktop tells you about.
    void offered();

private:
    void change(const std::function<void(UpdateSettings &)> &edit);

    UpdateChecker *m_checker;
    SettingsStore *m_settings;
    Opener *m_opener;
    QString m_putOff; // the version you said Later to
};

} // namespace pou
