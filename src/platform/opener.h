// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QUrl>
#include <memory>

namespace pou {

// What the app asks the desktop to do outside its own windows: open a link in
// the program that handles it, and copy text to the clipboard. Both only
// happen when the user clicks for them.
class Opener
{
public:
    virtual ~Opener() = default;

    virtual void open(const QUrl &url) = 0;
    virtual void copy(const QString &text) = 0;
};

// The one for this desktop.
std::unique_ptr<Opener> createOpener();

} // namespace pou
