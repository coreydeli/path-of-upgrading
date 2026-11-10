// SPDX-License-Identifier: MIT
#include "platform/win/winwindows.h"

#include <QFileInfo>
#include <iterator>

namespace pou {

DWORD processOf(HWND window)
{
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    return process;
}

WindowInfo describeWindow(HWND window)
{
    WindowInfo info;
    if (!window)
        return info;
    wchar_t className[256];
    if (const int length = GetClassNameW(window, className, std::size(className)); length > 0)
        info.classes << QString::fromWCharArray(className, length);

    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processOf(window));
    if (!process)
        return info;
    wchar_t path[MAX_PATH];
    DWORD length = std::size(path);
    if (QueryFullProcessImageNameW(process, 0, path, &length))
        info.program = QFileInfo(QString::fromWCharArray(path, int(length))).fileName();
    CloseHandle(process);
    return info;
}

} // namespace pou
