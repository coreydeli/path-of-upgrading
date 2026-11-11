// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"
#include "core/updates.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <chrono>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

namespace pou {

// Asks GitHub for the project's releases, when the program starts and about
// once a day while it runs, and keeps the newest one that is newer than the
// version you have. It asks only while the settings say to check, unless
// check() is called, which is you asking. Betas count as releases when the
// settings include them, and a version you skipped is kept but not offered.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    // The network has to outlive it. The version is the one you have.
    explicit UpdateChecker(QNetworkAccessManager *network, const Version &have = buildVersion(),
                           QObject *parent = nullptr);
    ~UpdateChecker() override;

    // How long between the checks, a day unless a test says otherwise.
    void setPeriod(std::chrono::milliseconds period) { m_timer.setInterval(period); }

    // The settings it goes by. A change to the betas or the version skipped
    // works out the newest again from the last answer, without asking.
    void follow(const UpdateSettings &settings);

    // Asks now, if the settings say to, and again every period.
    void start();
    // Asks now, whatever the settings say. Does nothing while it's asking.
    void check();

    const Version &have() const { return m_have; }
    bool isChecking() const { return !m_reply.isNull(); }
    // The newest release newer than yours, if there is one, skipped or not.
    const std::optional<Release> &newest() const { return m_newest; }
    // Whether that's the version you skipped.
    bool isSkipped() const;
    // Whether there's a newer release you haven't skipped.
    bool isOffered() const { return m_newest && !isSkipped(); }
    // When GitHub last answered, invalid before it has.
    QDateTime checked() const { return m_checked; }
    // Why the last check didn't get an answer, or empty when it did.
    QString errorString() const { return m_error; }

Q_SIGNALS:
    // Anything the above says has changed.
    void changed();
    // A check ended with a release to offer, which wasn't skipped.
    void found(const Release &release);

private:
    void ask();
    void answered();
    void pick();
    void fail(const QString &why);

    QNetworkAccessManager *m_network;
    Version m_have;
    UpdateSettings m_settings;
    QTimer m_timer;
    QPointer<QNetworkReply> m_reply;
    QList<Release> m_releases;
    std::optional<Release> m_newest;
    QDateTime m_checked;
    QString m_error;
};

} // namespace pou
