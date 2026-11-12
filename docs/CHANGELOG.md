# Changelog

Here is what changed in each version of Path of Upgrading, in plain words. The
format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the
version numbers follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Betas come out first, as pre-releases. Their changes are listed under the next
version that comes out.

## [Unreleased]

### Added

- The gear notes say the most each slot can roll, such as "Life (up to 149)" in
  the missing stats, and "Look for" gives it too, as "Life 55 or more (up to
  149)". An empty slot's "Wants most" shows it the same way. The figures come
  from Path of Building's data for Path of Exile 2.
- The slot panel on a wide gear page has a Can roll line under Look for. It lists up
  to six stats the build wants most in that slot, each with the top tier's range and its
  item level, such as "Life 100 to 149, item level 82". It's left out for weapons,
  off hands, jewels, flasks and charms.
- The character you play is picked in a Character box at the top of Your Gear, which shows
  its build.
- An About page in the sidebar's Settings group. It shows the version and edition, the
  license, the Qt and KDE Frameworks versions, where Path of Building was found, links to the
  source, the releases and the issue tracker, and the credits, with the fonts the
  app carries.
- A Help page in the sidebar's Settings group. It opens the wiki, or one of its pages, in
  your browser.
- The app looks on GitHub for a newer release when it starts, and about once a
  day while it runs. A dialog called "A New Version Is Out" offers Update Now,
  Later or Skip This Version. With the window hidden, a notification says so
  instead. Turn the check off, or include betas, in the Updates card on the
  Settings page.
- Update Now downloads the file for how you installed the app and checks it
  against the release's checksums before anything else happens. The AppImage
  replaces itself and restarts, and the Windows setup runs quietly. Debian and
  Fedora packages go through PackageKit, and show the command to run when it
  isn't there or fails. Arch shows the command to run. A build from source opens
  the release page.

### Changed

- The app has a new look, drawn by the app itself, so it looks the same on
  every desktop: its own controls, colors and fonts, with cut-corner cards. The
  text is in Spectral and the headings in Big Shoulders Display. The popup, the
  results window, the pages and the sidebar all have it.
- The app uses the game's words. It says equip, Unequip, Equipped by and Equip as in place
  of wear, Take Off and Worn, and Off Hand for the off hand. The gear score's stats are spelled
  out, such as Fire Resistance, Energy Shield Recharge and Movement Speed, and the tweaks you
  saved move to the new names. Prices converted in the results are in full, such as "about 3
  Divine Orbs". The Gear Grade page is Gear Score again.
- The gear page's "Your Resistances" card is now called "Resistances". A line
  under its heading says where the numbers come from, and the line of values
  stays hidden until they are known.
- "Typed in off your character sheet" is now "Type them in from your character
  sheet", with the numbers the game's sheet shows, over the maximum too.
- "Margin over the maximum" is now "Safety buffer". An item that leaves a
  resistance closer to its maximum than this gets a warning.
- "Maximum resistance" is now four rows: Fire, Cold, Lightning and Chaos
  maximum, each from 1 to 100. An imported build's maximums are used until you
  change one, and a single maximum saved by an earlier version is used for all
  four.
- On the Trade page, "Update poe.ninja's prices every" is now "Update poe.ninja's
  prices automatically", with minutes beside the number.
- A beta is no longer made on every merge into beta. It comes out when it's
  made by hand, or each night when there's new code. Changes to the docs alone
  don't count.
- In the gear score profiles, "Weapon DPS" is now "Weapon damage per second".
- The sidebar's Gear group is now called Gear Settings.
- The Characters page is now called Import Character, and it is in Gear Settings. The window
  opens on Your Gear rather than on the Characters page.
- Build Review's "Look Again" is now "Update", and it also reads the builds
  you've saved in Path of Building since.
- The gear score is now weighed by Path of Building for your build, and kept with
  the character. The Gear Score page has no build profile list. "Judge by my stat
  weights alone" is a switch in place of the None profile, and Weigh Again weighs
  the build again.
  The popup, the Characters page, the import message and the upgrade search no
  longer name a build profile.

### Fixed

- A weapon's damage is no longer listed as missing in a slot's gear notes.
- With a two-handed weapon in the main hand, the off hand no longer counts
  toward the gear score, and the popup no longer offers to wear an off-hand
  item there, unless the character can hold one beside it.

## [0.6.0] - 2026-11-10

[Release notes](https://github.com/coreydeli/path-of-upgrading/releases/tag/v0.6.0)

### Added

- Each release now includes a package for Fedora (an RPM file), next to the
  Arch and Debian packages.
- Each release includes an AppImage,
  `PathOfUpgrading-<version>-x86_64.AppImage`, for most current distros. Make
  it executable and run it.
- In a beta, a waystone or a precursor tablet is rated by its mods when you
  check it, with what each one gives and what it costs. It is experimental, and
  a release shows a waystone as before.

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
[0.6.0]: https://github.com/coreydeli/path-of-upgrading/releases/tag/v0.6.0
[0.5.0]: https://github.com/coreydeli/path-of-upgrading/releases/tag/v0.5.0
