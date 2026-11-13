# Path of Upgrading

An item overlay for Path of Exile 2 on Linux. Press a hotkey over an item in
the game and a popup next to it says whether the item is an upgrade for your
build. It can also search the trade site for better items, look for items like
the one you checked, and find bases to craft on. Trade searches are in the
betas only.

It is a native Qt 6 application, written in C++ and QML with Kirigami. KDE
Plasma on Wayland is the first supported desktop, and there is a Windows
version too. Other desktops may come later.

Get it from the [releases](https://github.com/coreydeli/path-of-upgrading/releases),
or build it from source, as [Installing](#installing) says. Betas come out
first, as pre-releases, for anyone who wants the newest changes before they're
released.
The [changelog](docs/CHANGELOG.md) says what changed in each version.

## Features

- Compares the item you check with the one you have equipped in the same slot. The
  verdict goes by Path of Building once you have imported a build, by a gear
  score for your build, or by your own stat weights. A ring, charm or jewel
  also shows what it would do in each place it fits.
- Works without a build too. Equip gear or set stat weights before you add a
  character, and the app makes one called My character and plays it.
- Opens a setup guide the first time the window opens, for your league, trade
  searches (in a beta), a first character and the key that checks an item.
  Start the program with `path-of-upgrading --setup` to see it again.
- Judges what an item costs you as well as what it gives. A big drop in
  damage, Effective HP, Life or Energy Shield counts against it, and "Never
  lose damage" and "Never lose defences" make either one mean an item is never
  an upgrade.
- Rates the mods on a waystone or a precursor tablet when you check it, in a
  beta only and still experimental. For each mod the popup says what it gives
  you and how dangerous it is to run, and it suggests what to do with the bad
  ones, such as an Orb of Annulment with its odds. It counts your character's
  resistances and life when it knows them. A release shows a waystone as before.
- The gear score comes from your build. With Path of Building on, it weighs
  what each mod is worth to your build in each slot, and keeps the result with
  your character, so the score works without Path of Building running. Until
  then, and for jewels, a start profile picked from the build is used: life and
  resistances, Energy Shield spellcaster, Energy Shield attacker, or minion
  summoner on Energy Shield. You can change how much each stat adds under
  Affix Impact, from Not at all (0x) to Three times (3x). Stat weights can be edited or suggested from
  the gear you have equipped, and "Judge by my stat weights alone" judges items
  by them with no gear score. Stats you don't care about can be ignored or left
  out of searches.
- Imports builds from Path of Building: from a code, a link, a file, or one
  you saved in Path of Building's own folder. It runs Path of Building without
  its window, so your build's own numbers judge the item. The import picks the
  start profile that fits the build, until Path of Building has weighed it.
- Finds Path of Building as the Flatpak from Flathub
  (`community.pathofbuilding.PathOfBuilding`), in your data folder, from its
  installer on Windows, or in a folder you choose. When it can't be found or
  won't work, the Build Review page says why and what to do, with buttons
  for Discover, Flathub, the install command and, on Windows, the download page.
- Searches the trade site from the popup for upgrades, for items with the same
  stats or better, for items that are better by your weights, and for bases to
  craft on. Searches stay within your budget, in the currency you pick, and
  one that finds nothing is tried again a little looser. While trade searches
  are on, poe.ninja's prices are updated automatically, every 10 minutes unless
  you set another number of minutes, from 1 to 60, on the Trade page.
- In a beta, shows the listings a search found in a window over the game, opened
  from the popup or with the Results hotkey. Each says whether it is an upgrade
  for the character you play, by how much and what it changes, the way the popup
  does, with its price, what that comes to in exalts or divines, the seller and
  how long ago it was listed.
- Draws the popup and the results window in the app's own look, with cut-corner
  panels. The popup's title takes the item's rarity color, and it has a verdict
  banner. Each gain or loss in the popup's table has a bar under its figure,
  sized against the biggest change. The popup fades in and rises a little when
  it opens, unless Decorations is Plain or the desktop has animations off.
- Trade searches are in the betas only, where they start on. They can be
  turned off, and set to the right league, including hardcore. Solo self-found
  turns them off, as there is no trading in it. Once you change that on the
  Trade page, your choice is kept. A release has no trade at all: no search
  buttons, no Trade page and no results window. It still has the league, with
  the Hardcore and Solo self-found switches, in the setup guide and on the
  Settings page.
- Keeps several characters, each with its own gear, build and weights, and can
  put the item you checked on your character. The gear page shows the gear
  score of each slot, the overall score, and what each slot needs to score
  more, and the most each slot can roll, such as Life (up to 149), from Path of
  Building's Path of Exile 2 data. On a wide window the slots are laid out like
  your character's equipment screen. Pick one to see what would improve it, the
  stats to look for in a better item, the top rolls the slot can have with their
  item levels (Can roll), and a Find an Upgrade search of the trade site when
  trade searches are on. Its Recommended Upgrades card lists up to three slots to
  upgrade first, with the stats to look for. The gear page also has your
  resistances, from your build or typed in from your character sheet, with a
  maximum for each element and a safety buffer, so an item that leaves one
  under the cap, or closer to it than the buffer, says so.
- Pick the character you play with the Character box at the top of Your Gear. You add
  characters and import builds under Import Character, in the sidebar's Gear Settings group.
- Spreads the settings pages across a wide window. Their cards flow into up to
  three columns, and stack again on a narrow one. The gear page stays in one
  column on a wide window, with the equipment screen under the overall score,
  and is a plain list of the slots on a narrow one.
- In a beta, can sign in to the trade site with your session ID, which is kept
  in your system keyring. The Trade page has the steps for finding it in your
  browser. Signed out, the site only takes simple searches, so upgrade searches
  look for a few of your build's key stats, and the results window rates each
  listing. Signed in, Path of Building weighs them for your build, the way its
  own trade searches do, or they go by your whole gear score.
- The popup can be dragged by its title to move it. When you copy an item
  yourself with Ctrl+C, it appears by the mouse.
- Takes the hotkeys only while Path of Exile 2 is running. The rest of the
  time other programs get them, so you can type them as usual.
- Stays in the system tray when you close the window. The tray icon shows the
  window, pauses the hotkeys and quits. Starting the program again shows the
  window that is already running, rather than opening a second one.
- Starts with your desktop from the first time it runs, with only the tray icon,
  as `path-of-upgrading --background` does. Turn that off with the switch on the
  Settings page, in the sidebar's Settings group, and it stays off. It tells you
  with a notification if the hotkeys couldn't be set up.
- Looks on GitHub for a newer release when it starts, and about once a day while
  it runs. When one is out, a dialog offers to update now, later or skip that
  version. With the window hidden, a notification says so instead. Update Now
  downloads the file for how you installed the app and checks it against the
  release's checksums. Nothing is installed until you press Update Now. Turn the
  check off, or include betas, under Updates on the Settings page.
- Has the app's own controls, colors and fonts, so it looks the same on every
  desktop. Its colors are the app's own dark ones, or your desktop's, which the
  Appearance page in the same group picks. It also picks the heading font
  (Big Shoulders Display by default), the text font (Spectral by default) and
  the text size. The text size follows your desktop's unless you
  set one, from 6 to 32 points. The font list only shows fonts that scale
  smoothly, each in its own face. Decorations there can be set to Full, the
  default, or Plain. Plain squares off the cut corners, leaves out the popup's
  fade and rise, and shows a listing's gear score as text rather than in a ring.
- Has an About page and a Help page in the sidebar's Settings group. About shows the version
  and edition, the license, the Qt and KDE Frameworks versions, where Path of Building was
  found, links to the source, the releases and the issue tracker, and the credits. Help opens
  the wiki, or one of its pages, in your browser.


## Installing

Each [release](https://github.com/coreydeli/path-of-upgrading/releases) has
these files:

| System | File |
| --- | --- |
| Arch Linux, and distros built from it | `path-of-upgrading-<version>-1-x86_64.pkg.tar.zst` |
| Debian 13 | `path-of-upgrading_<version>-1_amd64.deb` |
| Fedora, with KDE Frameworks 6 | `path-of-upgrading-<version>-1.fc<N>.x86_64.rpm` |
| Most current distros, as an AppImage | `PathOfUpgrading-<version>-x86_64.AppImage` |
| Windows 10 and 11, installed | `path-of-upgrading-<version>-setup.exe` |
| Windows 10 and 11, from any folder | `path-of-upgrading-<version>-windows.zip` |
| Any other Linux, to build yourself | `path-of-upgrading-<version>.tar.gz` |

The newest release is at the top of the list. A beta's version is like
`0.7.0beta2` in the Arch package and `0.7.0.beta.2` in the Debian and Fedora
ones. In the Fedora file name, `<N>` is the Fedora release it was built on.
The AppImage and the Windows files say `0.7.0-beta.2`.

### Arch Linux

The package is built against the newest Arch packages, so update first, then
install the file you downloaded:

```bash
sudo pacman -Syu
sudo pacman -U ./path-of-upgrading-*-x86_64.pkg.tar.zst
```

pacman installs what it needs with it.

The app doesn't install Arch packages itself. When a newer release is out,
Update Now downloads the new package and shows the `sudo pacman -U` command for
it. Run that yourself.

### Debian 13

apt installs the package with what it needs, when you give it the file's path:

```bash
sudo apt install ./path-of-upgrading_*_amd64.deb
```

It is made on Debian 13. Distros based on Debian or Ubuntu with Qt 6.8 and KDE
Frameworks 6.13 or newer may take it too, but only Debian 13 is tested. If apt
can't find what it depends on, build it from source instead.

When a newer release is out, Update Now downloads the new package and asks
PackageKit to install it, with your desktop's password prompt. The update
window then offers Restart Now. If PackageKit isn't there or fails, the window
shows the `sudo apt install` command to run instead.

### Fedora

The package is built on the newest Fedora, so update first, then install the
file you downloaded:

```bash
sudo dnf upgrade
sudo dnf install ./path-of-upgrading-*.rpm
```

dnf installs what it needs with it. It needs Fedora with KDE Frameworks 6.
CentOS Stream and RHEL don't have those, so they aren't covered. There is no
package repository yet. Update Now downloads each new file for you and asks
PackageKit to install it, with your desktop's password prompt. The update
window then offers Restart Now. If PackageKit isn't there or fails, it shows
the `sudo dnf install` command to run instead.

### AppImage

The AppImage runs on most current distros, with nothing installed first. It
needs glibc 2.41 or newer, which Debian 13, Fedora 42, Ubuntu 25.04 and
current Arch Linux have. Ubuntu 24.04 and Debian 12 are too old, so build from
source there. Make the file executable, then run it:

```bash
chmod +x PathOfUpgrading-*-x86_64.AppImage
./PathOfUpgrading-*-x86_64.AppImage
```

An AppImage mounts itself with FUSE. If it won't start because FUSE is
missing, as in a container, start it with `--appimage-extract-and-run`:

```bash
./PathOfUpgrading-*-x86_64.AppImage --appimage-extract-and-run
```

The AppImage updates itself. When a newer release is out, Update Now downloads
it, checks it, puts it in place of the file you started and restarts the app.
The folder the AppImage is in must be writable, as the new file is written next
to it. With "Start with your desktop" on, the autostart entry starts the
AppImage file, so if you move the file, turn the switch off and on again.

### Other Linux distros

Build it from the source in the release, or from a clone of the repository.
Install the libraries in [Requirements](#requirements) with your distro's
package manager first. `.github/scripts/install-arch.sh` and
`.github/scripts/install-debian.sh` list the package names on Arch and Debian,
which are close to the names on most distros. On Fedora,
`.github/scripts/install-fedora.sh` installs what the RPM spec needs.

```bash
tar -xf path-of-upgrading-<version>.tar.gz
cd path-of-upgrading-<version>
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix ~/.local
```

That installs it for you alone, with its entry in the app menu. Use
`sudo cmake --install build` instead to install it for everyone, in
`/usr/local`. Update Now opens the release page on GitHub instead, so you build
the new version yourself.

### Windows

Download the setup exe and open it. It installs for you alone, so it doesn't
ask for an administrator. It puts the app in
`AppData\Local\Programs\Path of Upgrading`, with an entry in the Start menu.

The zip has the same program and needs no installing. Unzip it anywhere and
open `path-of-upgrading.exe` in the `Path of Upgrading` folder.

When a newer release is out, Update Now runs the new setup exe quietly. That
installs the new version and starts it. The zip gets the setup too, so the new
version goes into `AppData\Local\Programs\Path of Upgrading`, and the copy you
unzipped stays as it was.

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
- Fedora: `sudo dnf remove path-of-upgrading`
- Built from source: `xargs rm < build/install_manifest.txt` in the source
  folder, with `sudo` if you installed it for everyone
- Windows, installed: Settings, then Apps, then Path of Upgrading
- Windows, from the zip: delete the folder

Your settings and characters are kept, in the folders under
[Your data](#your-data) on Linux, and in AppData on Windows. On Linux the
autostart entry stays too, so turn off Start with your desktop before you
uninstall.

## Requirements

Path of Upgrading needs a Linux desktop with KDE Plasma on Wayland. The game
runs under Proton as an X11 window, so the app reads it through XWayland.

To build it:

- a C++20 compiler, CMake 3.21 and Ninja
- Qt 6.8 or newer, with Core, Gui, Network, Widgets, Qml, Quick, Quick
  Controls 2, DBus, Test and QuickTest
- KDE Frameworks 6.13 or newer: Kirigami, Declarative, ColorScheme, I18n,
  GlobalAccel, GuiAddons, Notifications, StatusNotifierItem, WindowSystem
  and DBusAddons, and Config for the tests
- Kirigami Addons 1.7 or newer
- LayerShellQt 6
- Qt6Keychain
- the xcb libraries: xcb, xcb-keysyms, xcb-randr, xcb-xfixes and xcb-xtest

The minimum versions are the ones Debian 13 has.

To use it:

- a secret service for the keyring, such as KWallet, if you want to save a
  session ID in a beta
- a system tray that shows StatusNotifierItem icons, which Plasma does, for
  the tray icon
- the Qt Quick Shapes QML module, which the app loads when it runs. On Debian
  13 it's `qml6-module-qtquick-shapes`, which the Debian package depends on.
- the LayerShellQt Wayland plugin, `layer-shell-qt`, which the popup needs to
  show over the game. The Arch, Debian and Fedora packages depend on it, and
  the AppImage includes it.
- the Qt SVG icon plugin, which draws the app's icons. On Debian 13 it's
  `qt6-svg-plugins`. The Debian package depends on it, so a plain install gets
  it too.
- CA certificates, for the HTTPS requests to the trade site, poe.ninja and
  GitHub. On Debian 13 it's `ca-certificates`, which the Debian package depends
  on as well.
- [Path of Building Community (PoE2)](https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2)
  for Path of Exile 2, if you want to import builds. The easiest way is the
  Flatpak: `flatpak install flathub community.pathofbuilding.PathOfBuilding`.
  Open it once for Path of Exile 2, so it downloads what it needs. Other
  installs work too, with LuaJIT installed, and you can point the app at a
  folder on the Build Review page.

## Building

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run it from the build folder with `build/src/path-of-upgrading`, or install it
with `cmake --install build --prefix ~/.local`. That also installs the desktop
file, the icon and the AppStream metainfo.

Each release on GitHub has an Arch Linux package, a Debian 13 package, a
Fedora package and an AppImage. To make them yourself, run `makepkg` in
`packaging/arch`, or `cpack` in the build folder on Debian. On Fedora,
`.github/scripts/install-fedora.sh` installs what the RPM needs, and `rpmbuild`
builds it from `packaging/rpm/path-of-upgrading.spec`, with the source tarball
in `~/rpmbuild/SOURCES` as CI does. `.github/scripts/package-appimage.sh`
makes the AppImage from a release build on Debian 13.

Add `-DPOU_BETA=ON` to the configure step to build a beta, as the beta releases
are. Trade searches are only in a beta, where they start on. A build without
it has no trade at all.

## Releases

Work is done on short branches off `beta` and merged back into it. A
pre-release, like `v0.7.0-beta.2`, with the packages and the changes since the
last one, is made from `beta` by hand (Run workflow on the Release workflow) or
each night when `beta` has new code. Docs alone don't count, and a merge into
`beta` doesn't make one. Merging `beta` into `main` releases the version
in `CMakeLists.txt` as the latest release, and the next beta needs a new
version. The version is also in the metainfo's newest release, the PKGBUILD and
the RPM spec, and CI checks they all say the same.

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
  checked. A `history.json` that can't be read is kept beside it as
  `history.json.unreadable`, and a new one is started.
- `~/.cache/path-of-upgrading` has what it learned from the trade site and
  poe.ninja, and any update it downloaded, in `updates`

The app writes an entry to `~/.config/autostart` the first time it starts, so it
can start with your desktop. You can turn that off with the Start with your
desktop switch, or in System Settings. Removing the package doesn't remove the
entry, so turn the switch off before you uninstall.

The session ID is not in any of them. It is in the system keyring, and only a
beta reads it.

## Other desktops

The popup is drawn over the game as a layer shell surface, which needs a
compositor that has layer shell, like KWin. GNOME's Wayland session doesn't
have it, so the popup can't be shown over the game there. On an X11 session the
popup is an override-redirect window instead. The hotkeys are global shortcuts
from KDE, taken only while the game runs. While it's closed they aren't in
System Settings' shortcut list, so change them in the app. Where the app can't
see the game's windows, as on Wayland without Xwayland, they stay on all the
time, and System Settings lists them.

Closing the window leaves the program running in the tray. On a desktop with
no tray, such as GNOME without an extension for it, nothing shows that it is
still running. Starting it again brings the window back.

## Windows

[Installing](#windows) has the installer and the zip.

Path of Building is the one from its own installer,
[Path of Building Community (PoE2)](https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2/releases),
which goes in `%APPDATA%\Path of Building Community (PoE2)`. The app comes with
the LuaJIT that runs it. A copy you unzipped somewhere else can be picked on
the Build Review page.

Play in windowed fullscreen, as the popup can't be drawn over the game in
exclusive fullscreen. The hotkeys are changed in the app, on the Settings page
in the sidebar's Settings group, which also has the switch to start it when
you sign in. The hotkeys only work while the game runs. In a beta, session
IDs are kept in the Windows Credential Manager.

## Credits

- [Path of Building Community](https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2)
  does the calculations. Path of Upgrading doesn't include it or change it. It
  runs the copy you have installed, without its window.
- [rusty-path-of-building](https://github.com/meehl/rusty-path-of-building)
  (MIT) is what makes Path of Building run on Linux, and it is the Flatpak on
  Flathub. Path of Upgrading finds Path of Building where it puts it.
- The headings are set in [Big Shoulders Display](https://github.com/xotypeco/big_shoulders)
  and the text in [Spectral](https://github.com/productiontype/Spectral), by their
  project authors (SIL Open Font License 1.1, in `data/fonts`), which the app
  carries. The Appearance page can set other fonts.
- Currency prices come from [poe.ninja](https://poe.ninja), and the listings
  from the official [Path of Exile trade site](https://www.pathofexile.com/trade2).
- Path of Exile is a trademark of Grinding Gear Games. This project isn't
  affiliated with or endorsed by them.

## License

MIT, see [LICENSE](LICENSE). The fonts are under the SIL Open Font License 1.1,
see [OFL-Spectral.txt](data/fonts/OFL-Spectral.txt) and
[OFL-BigShoulders.txt](data/fonts/OFL-BigShoulders.txt).
