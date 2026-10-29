# Path of Upgrading

An item overlay for Path of Exile 2 on Linux. Press a hotkey over an item in
the game and a popup next to it says whether the item is an upgrade for your
build. It can also search the trade site for better items, look for items like
the one you checked, and find bases to craft on.

It is a native Qt 6 application, written in C++ and QML with Kirigami. KDE
Plasma on Wayland is the first supported desktop. Other desktops and Windows
may come later.

This is version 0.1.0 and there are no releases yet, so for now you build it
from source.

## Features

- Compares the item you check with the one you wear in the same slot. The
  verdict goes by a gear score for your build, by your own stat weights, or by
  Path of Building once you have imported a build.
- The gear score uses a build profile and lets you say how much each stat
  counts, from not at all to three times as much. Stat weights can be edited
  or suggested from the gear you wear, and stats you don't care about can be
  ignored or left out of searches.
- Imports builds from Path of Building: from a code, a link, a file, or one
  you saved in Path of Building's own folder. It runs Path of Building without
  its window, so your build's own numbers judge the item.
- Finds Path of Building as the Flatpak from Flathub
  (`community.pathofbuilding.PathOfBuilding`), as other installs of
  rusty-path-of-building, or in a folder you choose.
- Searches the trade site from the popup for upgrades, for items with the same
  stats or better, for items that are better by your weights, and for bases to
  craft on. Searches stay within your budget, in the currency you pick, and
  one that finds nothing is tried again a little looser.
- Shows the listings a search found in a window over the game, opened from the
  popup or with the Results hotkey. Each says whether it is an upgrade for the
  character you play, by how much and what it changes, the way the popup does,
  with its price, what that comes to in exalts or divines, the seller and how
  long ago it was listed.
- Trade searches can be turned off, and set to the right league, including
  hardcore. Solo self-found turns them off, as there is no trading in it.
- Keeps several characters, each with its own gear, build and weights, and can
  put the item you checked on your character.
- Can sign in to the trade site with your session ID, which lets the site take
  bigger searches. It is kept in your system keyring.
- Stays in the system tray when you close the window. The tray icon shows the
  window, pauses the hotkeys and quits. Starting the program again shows the
  window that is already running, rather than opening a second one.
- Can start with your desktop, from a switch on the Popup and Hotkeys page. It
  then starts with only the tray icon, as `path-of-upgrading --background`
  does. It tells you with a notification if the hotkeys couldn't be set up.

## Requirements

Path of Upgrading needs a Linux desktop with KDE Plasma on Wayland. The game
runs under Proton as an X11 window, so the app reads it through XWayland.

To build it:

- a C++20 compiler, CMake 3.21 and Ninja
- Qt 6.8 or newer, with Core, Gui, Network, Widgets, Qml, Quick, Quick
  Controls 2, DBus, Test and QuickTest
- KDE Frameworks 6.13 or newer: Kirigami, Declarative, I18n, GlobalAccel,
  Notifications, StatusNotifierItem, WindowSystem and DBusAddons, and Config
  for the tests
- Kirigami Addons 1.7 or newer
- LayerShellQt 6
- Qt6Keychain
- the xcb libraries: xcb, xcb-keysyms, xcb-randr, xcb-xfixes and xcb-xtest

The minimum versions are the ones Debian 13 has.

To use it:

- a secret service for the keyring, such as KWallet, if you want to save a
  session ID
- a system tray that shows StatusNotifierItem icons, which Plasma does, for
  the tray icon
- [Path of Building Community](https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2)
  for Path of Exile 2, if you want to import builds. The easiest way is the
  Flatpak: `flatpak install flathub community.pathofbuilding.PathOfBuilding`.
  Open it once for Path of Exile 2, so it downloads what it needs. Other
  installs work too, with LuaJIT installed, and you can point the app at a
  folder on the Path of Building page.

## Building

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run it from the build folder with `build/src/path-of-upgrading`, or install it
with `cmake --install build --prefix ~/.local`. That also installs the desktop
file, the icon and the AppStream metainfo.

## Tests

```bash
cd build && ctest -j8
```

The window tests are offscreen. They don't touch your clipboard, your
keyring, your shortcuts or the network. Run them on a private session bus
(`dbus-run-session -- ctest -j8`) so they can't start any desktop services.
Two tests, `tst_xmonitors` and `tst_xwindows`, read the real XWayland, so they
fail while the screens are off.

## Your data

- `~/.config/path-of-upgradingrc` has the settings
- `~/.local/share/path-of-upgrading` has your characters and the items you
  checked
- `~/.cache/path-of-upgrading` has what it learned from the trade site and
  poe.ninja

If you turn on Start with the desktop, there is also an entry in
`~/.config/autostart`, which you can turn off in System Settings too.

The session ID is not in any of them. It is in the system keyring.

## Other desktops

The popup is drawn over the game as a layer shell surface, which needs a
compositor that has layer shell, like KWin. GNOME's Wayland session doesn't
have it, so the popup can't be shown over the game there. On an X11 session the
popup is an override-redirect window instead. The hotkey is a global shortcut
from KDE, so it can be changed in System Settings as well as in the app.

Closing the window leaves the program running in the tray. On a desktop with
no tray, such as GNOME without an extension for it, nothing shows that it is
still running. Starting it again brings the window back.

## Credits

- [Path of Building Community](https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2)
  does the calculations. Path of Upgrading doesn't include it or change it. It
  runs the copy you have installed, without its window.
- [rusty-path-of-building](https://github.com/meehl/rusty-path-of-building)
  (MIT) is what makes Path of Building run on Linux, and it is the Flatpak on
  Flathub. Path of Upgrading finds Path of Building where it puts it.
- Currency rates come from [poe.ninja](https://poe.ninja), and the listings
  from the official [Path of Exile trade site](https://www.pathofexile.com/trade2).
- Path of Exile is a trademark of Grinding Gear Games. This project isn't
  affiliated with or endorsed by them.

## License

MIT, see [LICENSE](LICENSE).
