# Path of Upgrading

An item overlay for Path of Exile 2 on Linux. Press a hotkey over an item in
the game and a popup next to it says whether the item is an upgrade for your
build. It can also search the trade site for better items, look for items like
the one you checked, and find bases to craft on.

It is a native Qt 6 application, written in C++ and QML with Kirigami. KDE
Plasma on Wayland is the first supported desktop, and there is a Windows
version too. Other desktops may come later.

Get it from the [releases](https://github.com/coreydeli/path-of-upgrading/releases),
or build it from source, as [Installing](#installing) says. Betas come out
first, as pre-releases, for anyone who wants the newest changes before they're
released.

## Features

- Compares the item you check with the one you wear in the same slot. The
  verdict goes by Path of Building once you have imported a build, by a gear
  score for your build, or by your own stat weights.
- Works without a build too. Wear gear or set stat weights before you add a
  character, and the app makes one called My character and plays it.
- Judges what an item costs you as well as what it gives. A big drop in
  damage, Effective HP, Life or Energy Shield counts against it, and "Never
  lose damage" and "Never lose defences" make either one mean an item is never
  an upgrade.
- The gear score uses a build profile: life and resistances, Energy Shield
  spellcaster, Energy Shield attacker, or minion summoner. You can say how much
  each stat counts, from not at all to three times as much. Stat weights can
  be edited or suggested from the gear you wear, and stats you don't care
  about can be ignored or left out of searches.
- Imports builds from Path of Building: from a code, a link, a file, or one
  you saved in Path of Building's own folder. It runs Path of Building without
  its window, so your build's own numbers judge the item. The import picks the
  profile that fits the build.
- Finds Path of Building as the Flatpak from Flathub
  (`community.pathofbuilding.PathOfBuilding`), in your data folder, from its
  installer on Windows, or in a folder you choose. When it can't be found or
  won't work, the Build Analysis page says why and what to do, with buttons
  for Discover, Flathub, the install command and, on Windows, the download page.
- Searches the trade site from the popup for upgrades, for items with the same
  stats or better, for items that are better by your weights, and for bases to
  craft on. Searches stay within your budget, in the currency you pick, and
  one that finds nothing is tried again a little looser. While trade searches
  are on, poe.ninja's rates are updated every 10 minutes, or as often as you
  set on the Trade page.
- Shows the listings a search found in a window over the game, opened from the
  popup or with the Results hotkey. Each says whether it is an upgrade for the
  character you play, by how much and what it changes, the way the popup does,
  with its price, what that comes to in exalts or divines, the seller and how
  long ago it was listed.
- Trade searches can be turned off, and set to the right league, including
  hardcore. Solo self-found turns them off, as there is no trading in it. Betas
  start with them on and releases with them off. Once you change that on the
  Trade page, your choice is kept.
- Keeps several characters, each with its own gear, build and weights, and can
  put the item you checked on your character. The gear page shows the gear
  score of each slot, the overall score, and what each slot needs to score
  more. Its Recommended Upgrades card lists up to three slots to upgrade first,
  with the stats to look for in a better item. Each slot with an item also has
  Find an Upgrade, which searches the trade site when trade searches are on.
  The gear page also has your resistances, from your build or typed in off the
  character sheet, so an item that leaves one under the cap says so.
- Spreads the settings pages across a wide window. Their cards flow into up to
  three columns, and stack again on a narrow one. On the gear page, your
  resistances and the other worn items sit beside the main gear.
- Can sign in to the trade site with your session ID, which is kept in your
  system keyring. The Trade page has the steps for finding it in your browser.
  Signed out, the site only takes simple searches, so upgrade searches look for
  a few of your build's key stats, and the results window rates each listing.
  Signed in, Path of Building weighs them for your build, the way its own trade
  searches do, or they go by your whole gear score.
- The popup can be dragged by its title to move it. When you copy an item
  yourself with Ctrl+C, it appears by the mouse.
- Stays in the system tray when you close the window. The tray icon shows the
  window, pauses the hotkeys and quits. Starting the program again shows the
  window that is already running, rather than opening a second one.
- Starts with your desktop from the first time it runs, with only the tray
  icon, as `path-of-upgrading --background` does. Turn that off with the switch
  on the Settings page, and it stays off. It tells you with a notification if
  the hotkeys couldn't be set up.
- Has dark colors of its own, as a KDE color scheme, or follows your desktop's
  colors, from the Appearance page. The same page picks the heading font
  (Cinzel by default), the text font and the text size. The text size follows
  your desktop's unless you set one, from 6 to 32 points. The font list only
  shows fonts that scale smoothly, each in its own face. Decorations there can
  be set to Plain, which leaves out the glow behind the pages, the corner
  pieces and fade of the popup, and the rings round gear scores.


## Installing

Each [release](https://github.com/coreydeli/path-of-upgrading/releases) has
these files:

| System | File |
| --- | --- |
| Arch Linux, and distros built from it | `path-of-upgrading-<version>-1-x86_64.pkg.tar.zst` |
| Debian 13 | `path-of-upgrading_<version>-1_amd64.deb` |
| Windows 10 and 11, installed | `path-of-upgrading-<version>-setup.exe` |
| Windows 10 and 11, from any folder | `path-of-upgrading-<version>-windows.zip` |
| Any other Linux, to build yourself | `path-of-upgrading-<version>.tar.gz` |

The newest release is at the top of the list. A beta's version is like
`0.1.0beta2` in the Arch package and `0.1.0.beta.2` in the Debian one.

### Arch Linux

The package is built against the newest Arch packages, so update first, then
install the file you downloaded:

```bash
sudo pacman -Syu
sudo pacman -U ./path-of-upgrading-*-x86_64.pkg.tar.zst
```

pacman installs what it needs with it. For the Plasma look outside Plasma,
install `qqc2-desktop-style` too.

### Debian 13

apt installs the package with what it needs, when you give it the file's path:

```bash
sudo apt install ./path-of-upgrading_*_amd64.deb
```

It is made on Debian 13. Distros based on Debian or Ubuntu with Qt 6.8 and KDE
Frameworks 6.13 or newer may take it too, but only Debian 13 is tested. If apt
can't find what it depends on, build it from source instead.

### Other Linux distros

Build it from the source in the release, or from a clone of the repository.
Install the libraries in [Requirements](#requirements) with your distro's
package manager first. `.github/scripts/install-arch.sh` and
`.github/scripts/install-debian.sh` list the package names on Arch and Debian,
which are close to the names on most distros.

```bash
tar -xf path-of-upgrading-<version>.tar.gz
cd path-of-upgrading-<version>
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix ~/.local
```

That installs it for you alone, with its entry in the app menu. Use
`sudo cmake --install build` instead to install it for everyone, in
`/usr/local`.

### Windows

Download the setup exe and open it. It installs for you alone, so it doesn't
ask for an administrator. It puts the app in
`AppData\Local\Programs\Path of Upgrading`, with an entry in the Start menu.

The zip has the same program and needs no installing. Unzip it anywhere and
open `path-of-upgrading.exe` in the `Path of Upgrading` folder.

Neither is signed yet, so the first time you open one, SmartScreen says it
doesn't know the app. Choose More info, then Run anyway.

### Checking a download

Each release has a `SHA256SUMS` file. Download it into the same folder as the
files, then on Linux:

```bash
sha256sum --ignore-missing -c SHA256SUMS
```

On Windows, in PowerShell, compare what this prints with the line for the file
in `SHA256SUMS`:

```powershell
Get-FileHash path-of-upgrading-<version>-setup.exe
```

### Path of Building

Path of Building is what judges items with your build, and you install it on
its own. On Linux, the Flatpak from Flathub is the easiest:

```bash
flatpak install flathub community.pathofbuilding.PathOfBuilding
```

Open it once for Path of Exile 2, so it downloads what it needs. On Windows,
use the installer from
[Path of Building Community (PoE2)](https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2/releases).
The app finds either one by itself. Without Path of Building, items are judged
by the gear score or your stat weights.

### Uninstalling

- Arch Linux: `sudo pacman -R path-of-upgrading`
- Debian: `sudo apt remove path-of-upgrading`
- Built from source: `xargs rm < build/install_manifest.txt` in the source
  folder, with `sudo` if you installed it for everyone
- Windows, installed: Settings, then Apps, then Path of Upgrading
- Windows, from the zip: delete the folder

Your settings and characters are kept, in the folders under
[Your data](#your-data) on Linux, and in AppData on Windows. On Linux the
autostart entry stays too, so turn off Start with the desktop before you
uninstall.

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
  folder on the Build Analysis page.

## Building

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run it from the build folder with `build/src/path-of-upgrading`, or install it
with `cmake --install build --prefix ~/.local`. That also installs the desktop
file, the icon and the AppStream metainfo.

Each release on GitHub has an Arch Linux package and a Debian 13 package. To
make them yourself, run `makepkg` in `packaging/arch`, or `cpack` in the build
folder on Debian.

Add `-DPOU_BETA=ON` to the configure step to build a beta, as the beta releases
are. Trade searches start on in a beta and off without it.

## Releases

Work is done on short branches off `beta` and merged back into it. Each merge
into `beta` makes a pre-release, like `v0.1.0-beta.2`, with the packages and
the changes since the last one. Merging `beta` into `main` releases the version
in `CMakeLists.txt` as the latest release, and the next beta needs a new
version. The version is also in the metainfo's newest release and the PKGBUILD,
and CI checks they say the same.

## Tests

```bash
cd build && ctest -j8
```

The window tests are offscreen. They don't touch your clipboard, your
keyring, your shortcuts or the network. Run them on a private session bus
with no services, so they can't start any of the desktop's:

```bash
cd build && dbus-run-session --config-file=../tests/bus.conf -- ctest -j8
```

The tests of the X side read the X server in `DISPLAY`, and skip when there's
none. Two of them, `tst_xmonitors` and `tst_xwindows`, fail while the screens
are off. `.github/scripts/run-tests.sh` runs them all the way the CI does, with
an X server of their own from `xvfb-run`.

## Your data

- `~/.config/path-of-upgradingrc` has the settings
- `~/.local/share/path-of-upgrading` has your characters and the items you
  checked
- `~/.cache/path-of-upgrading` has what it learned from the trade site and
  poe.ninja

The app writes an entry to `~/.config/autostart` the first time it starts, so it
can start with your desktop. You can turn that off with the Start with the
desktop switch, or in System Settings. Removing the package doesn't remove the
entry, so turn the switch off before you uninstall.

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

## Windows

[Installing](#windows) has the installer and the zip.

Path of Building is the one from its own installer,
[Path of Building Community (PoE2)](https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2/releases),
which goes in `%APPDATA%\Path of Building Community (PoE2)`. The app comes with
the LuaJIT that runs it. A copy you unzipped somewhere else can be picked on
the Build Analysis page.

Play in windowed fullscreen, as the popup can't be drawn over the game in
exclusive fullscreen. The hotkeys are changed in the app, on the Settings
page, which also has the switch to start it when you sign in. Session
IDs are kept in the Windows Credential Manager.

## Credits

- [Path of Building Community](https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2)
  does the calculations. Path of Upgrading doesn't include it or change it. It
  runs the copy you have installed, without its window.
- [rusty-path-of-building](https://github.com/meehl/rusty-path-of-building)
  (MIT) is what makes Path of Building run on Linux, and it is the Flatpak on
  Flathub. Path of Upgrading finds Path of Building where it puts it.
- The headings are set in [Cinzel](https://github.com/NDISCOVER/Cinzel) by default, by The Cinzel Project Authors
  (SIL Open Font License 1.1, in `data/fonts`), which the app carries. The
  Appearance page can set another heading font.
- Currency rates come from [poe.ninja](https://poe.ninja), and the listings
  from the official [Path of Exile trade site](https://www.pathofexile.com/trade2).
- Path of Exile is a trademark of Grinding Gear Games. This project isn't
  affiliated with or endorsed by them.

## License

MIT, see [LICENSE](LICENSE). The Cinzel font is under the SIL Open Font License 1.1,
see [OFL.txt](data/fonts/OFL.txt).
