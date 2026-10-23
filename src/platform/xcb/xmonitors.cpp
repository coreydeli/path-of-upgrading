// SPDX-License-Identifier: MIT
#include "platform/xcb/xmonitors.h"

#include <utility>
#include <xcb/randr.h>

namespace pou {

QList<NamedScreen> xMonitors(XConnection &x)
{
    const xcb_query_extension_reply_t *randr = xcb_get_extension_data(x.xcb(), &xcb_randr_id);
    if (!randr || !randr->present)
        return {};
    // the ones that are showing something
    xcb_generic_error_t *error = nullptr;
    const XReply<xcb_randr_get_monitors_reply_t> reply(
        xcb_randr_get_monitors_reply(x.xcb(), xcb_randr_get_monitors(x.xcb(), x.root(), true), &error));
    std::free(error);
    if (!reply)
        return {};

    QList<NamedScreen> monitors;
    QList<xcb_get_atom_name_cookie_t> names;
    for (auto it = xcb_randr_get_monitors_monitors_iterator(reply.get()); it.rem > 0;
         xcb_randr_monitor_info_next(&it)) {
        monitors.append({.geometry = QRect(it.data->x, it.data->y, it.data->width, it.data->height)});
        names.append(xcb_get_atom_name(x.xcb(), it.data->name));
    }
    // all asked for before waiting on the first
    for (qsizetype i = 0; i < monitors.size(); ++i) {
        const XReply<xcb_get_atom_name_reply_t> name(xcb_get_atom_name_reply(x.xcb(), names.at(i), &error));
        std::free(std::exchange(error, nullptr));
        if (name) {
            monitors[i].name = QString::fromLatin1(xcb_get_atom_name_name(name.get()),
                                                   xcb_get_atom_name_name_length(name.get()));
        }
    }
    return monitors;
}

} // namespace pou
