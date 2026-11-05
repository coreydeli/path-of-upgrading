// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace pou {

// The page you sign in to the Path of Exile website on.
QUrl signInUrl();

// What the Trade page says to someone who has never looked for the session
// ID: the cookie of the browser that's signed in to the website.
struct SessionGuide
{
    QStringList steps; // how to find it, numbered
    QStringList notes; // what to know about it, a sentence each
};

SessionGuide guideToSessionId();

} // namespace pou
