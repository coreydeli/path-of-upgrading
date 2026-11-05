// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <chrono>

namespace pou {

class RatesStore;

// Asks poe.ninja for a league's rates again every few minutes, so they don't
// go stale while the app is left running. When poe.ninja lets it down it
// waits twice as long each time, up to an hour, and goes back to the usual
// wait once it answers.
class RatesRefresher : public QObject
{
    Q_OBJECT

public:
    // The store has to outlive it.
    explicit RatesRefresher(RatesStore *rates, QObject *parent = nullptr);

    // How long a minute is, which tests make short.
    void setMinute(std::chrono::milliseconds minute) { m_minute = minute; }

    // Keeps the league's rates fresh, by its trade site id. Doing so again
    // with the same league and wait carries on as it was. An empty league
    // stops it.
    void follow(const QString &league, int minutes);
    void stop();

    bool isRunning() const { return m_running; }
    // How many minutes until the next try, with the wait backed off.
    int waitMinutes() const;

private:
    void ask();
    void answered();
    void arm();

    RatesStore *m_rates;
    QTimer m_timer;
    std::chrono::milliseconds m_minute = std::chrono::minutes(1);
    QString m_league;
    int m_minutes = 0;  // between tries when all is well
    int m_failures = 0; // tries in a row that failed
    bool m_running = false;
    bool m_asked = false; // for the rates, and waiting for the answer
};

} // namespace pou
