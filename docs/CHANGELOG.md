# Changelog

Here is what changed in each version of Path of Upgrading, in plain words. The
format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the
version numbers follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Betas come out first, as pre-releases. Their changes are listed under the next
version that comes out.

## [Unreleased]

These changes are in the beta. The 0.6.0 betas, from
[0.6.0-beta.1](https://github.com/coreydeli/path-of-upgrading/releases/tag/v0.6.0-beta.1)
on, are pre-releases of this version.

### Added

- Each release now includes a package for Fedora (an RPM file), next to the
  Arch and Debian packages.
- Each release includes an AppImage,
  `PathOfUpgrading-<version>-x86_64.AppImage`, for most current distros. Make
  it executable and run it.

### Changed

- The hotkeys are only taken while Path of Exile 2 is running. The rest of the
  time other programs get them, so you can type them as usual.
- The sidebar has a Settings group, with Settings and Appearance in it, which
  works like the Gear group.
- A release has no trade at all: no Trade page, no results window and no
  search buttons on the popup. Trade searches are only in the betas. A release
  still has the league, with the Hardcore and Solo self-found switches, in the
  setup guide and on the Settings page.
- The Build Analysis page is now called Build Review.
- The sidebar stays on Gear while you move between Gear Grade, Stat Weights and
  Ignored Stats. Back still takes you up one level.

### Fixed

- When the game runs under Wine or Proton, the popup opens beside the mouse
  pointer. Before, it could jump to the top of the screen. It opens 20 pixels to
  the left of the pointer, or to the right when there is no room on the left.
- The Debian package now installs `layer-shell-qt`, the Wayland plugin the
  popup needs to show over the game. Without it, the popup couldn't be placed
  over the game on Wayland.

## [0.5.0] - 2026-11-05

[Release notes](https://github.com/coreydeli/path-of-upgrading/releases/tag/v0.5.0)

Path of Upgrading is an item overlay for Path of Exile 2. Press a hotkey over an
item in the game, and a popup says whether it is an upgrade for your build. It
can also search the trade site for better items, look for items like the one you
checked, and find bases to craft on. It runs on Linux, first on KDE Plasma under
Wayland, and on Windows 10 and 11.

The 0.1.0 betas (v0.1.0-beta.1 to v0.1.0-beta.33, from 2026-11-03 to
2026-11-05) and v0.5.0-beta.1 came before this release.

### Added

- A Windows version, with an installer and a zip file in each release. The
  files are not signed yet.
- A setup guide the first time the window opens. It asks for your league, your
  trade search choice, a first character and the key that checks an item.
- A trade results window. It shows the listings a search found, over the game.
  Each listing says whether it is an upgrade for the character you play, and how
  much it changes. Open it from the popup, or with the Results hotkey.
- Upgrade searches weighed by Path of Building, while you are signed in with
  your session ID and a build is loaded. Each search looks for what your build
  needs most.
- An Ignored Stats page, for the stats you don't care about. You can ignore a
  stat, or leave it out of searches.
- An Item History page, with the last 30 items you checked. You can check any of
  them again.
- Recommended Upgrades on the gear page. It lists up to three slots to upgrade
  first, with the stats to look for, and an overall gear score. On a wide window,
  the gear page shows your slots in the same layout as the equipment screen.
- A resistances card on the gear page. Type in your resistances from the
  character sheet, or use the ones from your build. Your gear is checked against
  them, so an item that leaves you under the cap says so.
- A tray icon. Closing the window keeps the app running in the tray. Starting it
  again shows the window that is already open, rather than a second copy.
- The app can start with your desktop. It is on from the first run, and you can
  turn it off in Settings.
- Your session ID is kept in your system keyring, not in the app's settings. The
  Trade page shows where to find it in your browser.
- The app has dark colors of its own, or it can use your desktop's colors. Pick
  the heading font, the text font and the text size on the Appearance page.
- You can drag the popup by its title to move it.
- The popup shows more of the numbers behind a verdict. It shows how much your
  damage and Effective HP change, and which resistances an item caps.
- A gear score profile for Energy Shield attackers. Importing a build picks the
  profile that fits it.
- An item that fits more than one place, such as a ring, shows what it would do
  in each place where you wear something, and which place is best.

### Changed

- Trade searches are on in betas and off in releases, until you turn them on in
  the Trade page. Your choice is kept.
- The popup stays up for 60 seconds by default. The setting is called Popup
  Timeout.
- The trade budget fields are called Lowest price and Highest price. While trade
  searches are on, poe.ninja's prices refresh every 10 minutes. You can set this
  from 1 to 60 minutes.
- Some pages have new names. The Popup and Hotkeys page is Settings, the Path of
  Building page is Build Analysis, the checked items page is Item History, and
  the Gear Score page is Gear Grade. Gear Grade, Stat Weights and Ignored Stats
  are under Gear in the sidebar.
- The verdict weighs losses by how big they are. A big drop in damage or
  defences now counts for more than a small drop in a resistance. A new
  "Never lose defences" switch on the Build Analysis page is on by default.
- A stat the build doesn't score says "Not scored", instead of "Not used". The
  gear score counts more utility stats, such as stun threshold and mana
  regeneration.
- The Affix Impact card shows the multiplier for each level, from Not at all (0x)
  to Three times (3x).
- The popup, the results window and the main window have a new look. Decorations
  can be set to Full or Plain.

### Fixed

- Signed out, the first upgrade search no longer sends searches that the trade
  site turns down, so it doesn't wait on them.
- A heavy build has more time to load in Path of Building, so the first check
  doesn't fail on a slower PC.
- When the popup is under your mouse, the hotkey hides it first. The game then
  copies the item you are checking, and not the popup.
- Gear you copy yourself with Ctrl+C shows the popup beside the mouse, as hotkey
  checks do.

[Unreleased]: https://github.com/coreydeli/path-of-upgrading/commits/beta
[0.5.0]: https://github.com/coreydeli/path-of-upgrading/releases/tag/v0.5.0
