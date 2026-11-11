// SPDX-License-Identifier: MIT
#pragma once

#include "platform/packageinstaller.h"

#include <QDBusConnection>
#include <QPointer>
#include <QString>
#include <optional>

namespace pou {

// Installs a downloaded .deb or .rpm through PackageKit, the way a software
// center does. PackageKit asks for your password in the desktop's own prompt.
// The file isn't from a repository, so it isn't trusted by default and
// PackageKit says so when it asks.
class PackageKitInstaller : public PackageInstaller
{
public:
    // PackageKit on the system bus.
    PackageKitInstaller();
    // Another bus and name, for a test. The system bus is only connected to
    // when something is installed.
    PackageKitInstaller(const QDBusConnection &bus, const QString &service);
    ~PackageKitInstaller() override;

    // The package is in place, and the running program is the old one.
    After after() const override { return After::Offer; }
    // Without PackageKit, or when it fails or you cancel, there's the command.
    bool fallsBackToCommand() const override { return true; }
    void install(const QString &file, const Done &done) override;

private:
    std::optional<QDBusConnection> m_bus;
    QString m_service;
    QPointer<QObject> m_running;
};

} // namespace pou
