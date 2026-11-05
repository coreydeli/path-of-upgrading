// SPDX-License-Identifier: MIT
#include "platform/opener.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>

namespace pou {
namespace {

class DesktopOpener : public Opener
{
public:
    void open(const QUrl &url) override { QDesktopServices::openUrl(url); }
    void copy(const QString &text) override { QGuiApplication::clipboard()->setText(text); }
};

} // namespace

std::unique_ptr<Opener> createOpener()
{
    return std::make_unique<DesktopOpener>();
}

} // namespace pou
