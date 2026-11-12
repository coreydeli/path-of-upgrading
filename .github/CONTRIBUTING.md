# Contributing to Path of Upgrading

Thanks for helping. This page covers how to report a problem, how to get the
code building, and what a change needs before it can be merged. The
[README](../README.md) has the requirements and how to build and test.

## Before you start

Linux comes first, with KDE Plasma on Wayland as the first desktop, and
there is a Windows version too. Anything specific to an operating system or a
desktop belongs in `src/platform`, and the rest of the code shouldn't depend
on it. The Windows code is in `src/platform/win`, behind `Q_OS_WIN`.

For anything bigger than a small fix, open an issue first and describe what
you want to change. That saves you work if it's out of scope or someone is
already on it.

## Reporting a bug or asking for a feature

Use the issue forms, and search the open issues first in case it's already
known.

For a bug, give the version, your desktop, how you installed the app and the
steps that cause the problem. If it's about an item, paste the item text.

Never post your session ID (POESESSID), your settings file or your own Path of
Building builds. Check any logs and screenshots for them before you post.

## Setting up

Install what the README lists under Requirements, then make a debug build:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The beta releases add `-DPOU_BETA=ON` to the configure step, which turns trade
searches on. Without it, a build has no trade at all. Add it when you're
testing that.

Run the tests on the private bus the README describes:

```bash
cd build && dbus-run-session --config-file=../tests/bus.conf -- ctest -j8
```

CI builds on Arch Linux, on Debian 13, on Fedora and on Windows in MSYS2,
checks the formatting, treats warnings as errors and runs every test. The
Fedora job also builds the RPM and checks it with rpmlint. When packages are
made, for a release or a pull request that changes how they're made, one job
installs each of the Arch, Debian and Fedora packages in a clean container of
that distribution with its own tool, to see that it starts, and another
installs and uninstalls the Windows setup program. The Debian job makes the
AppImage, and when packages are made another job starts it in clean Fedora,
Arch Linux and Ubuntu containers. A change has to pass all of it. The steps
are in `.github/workflows/ci.yml`.

The check named CI is the one `beta` requires before a pull request can merge.
It passes once every other job has passed or been skipped.

A change to nothing but Markdown files, the `docs` folder, `.gitignore` or the
issue forms skips those builds, and they show as skipped, which counts as a
pass. A push to `main` builds when it releases, or when it changes more than
the docs. A merge into `beta` doesn't build again, as its pull
request was checked. A beta release is made by hand, or each night when `beta`
has new code.

## Branches and pull requests

1. Branch from `beta`. Name the branch after its area and change, such as
   `trade/signed-out` or `pob/timeouts`.
2. Make small commits, each one a single step. Write each title as a short
   imperative sentence, such as "Keep the popup's place up to date as it
   grows". Don't reuse a commit title.
3. Open a pull request into `beta` and fill in the template, with its
   features, fixes and other changes. Keep each pull request to one change.
   Start its title with its kind and a colon: `Feature:`, `Bug:`, `Docs:`,
   `CI:` or `Maintenance:`, then a short sentence, such as "Feature: Spread
   the settings pages across a wide window". Give it the label that matches
   (`enhancement`, `bug`, `documentation`, `ci` or `maintenance`), as the
   release notes are grouped by those labels.
4. If `beta` moves on while you work, merge it into your branch. The check
   named CI also needs your branch up to date with `beta`, so merge it again
   when `beta` has moved on.
5. Delete your branch once it's merged.

The maintainer merges `beta` into `main` when there's a release, so you don't
need to.

## Code

The code is C++20 for Qt 6.8, the oldest version the app supports. Debian 13
ships Qt 6.8.2, KDE Frameworks 6.13 and Kirigami Addons 1.7, so don't use
anything newer than those.

- Format the files you changed with `clang-format -i`. The repository's
  `.clang-format` sets the style, with a column limit of 110.
- The build must have no warnings.
- Put the namespace `pou` around the code. Start each C++ file with
  `// SPDX-License-Identifier: MIT`.
- The build turns off implicit conversions from plain C strings to `QString`,
  so write text as `u"..."_s`, with `using namespace Qt::Literals::StringLiterals;`.
- The build also turns off `foreach` and the plain `signals`, `slots` and
  `emit` keywords. Use `Q_SIGNALS`, `Q_SLOTS` and `Q_EMIT`, and range-for.
- Put designated initializers in declaration order.
- Match the naming and idiom of the code around your change.

The [style guide](../docs/style-guide.md) has the rest, for the code and for the
text people read in the app and the docs.

The folders split the code like this:

| Folder | What goes there |
| --- | --- |
| `src/core` | Pure logic with no I/O: parsing items, scoring, trade queries, settings and characters |
| `src/services` | Network and processes: the trade site, poe.ninja and Path of Building |
| `src/platform` | The operating system: overlay windows, global shortcuts and reading the game, through XWayland on Linux |
| `src/app` | The QML app, its models in C++ and the QML in `src/app/qml` |

`data/slotmods.json` lists the mods each slot can roll, with their tiers, and
the gear notes read it. It comes from Path of Building's Path of Exile 2 data,
so make it again by hand after a game patch. Point `tools/slot-mods.py` at the
Path of Building folder that has `Data/ModItem.lua` and `Data/Bases/` in it,
and at the file to write:

```bash
python3 -I tools/slot-mods.py <PathOfBuilding2 folder> data/slotmods.json
```

The Flatpak keeps that folder under
`~/.var/app/community.pathofbuilding.PathOfBuilding/data/`. Nothing is
downloaded, so the folder must be there already. Commit the new file like any
other change.

Keep comments short and plain. Explain what isn't obvious from the code, and
don't narrate it or make up a reason for it.

On the settings pages, use the app's own form rows (`FormRow.qml`,
`SwitchRow.qml` and `NumberField`). Don't put `trailing:` on a FormCard
delegate: in Kirigami Addons 1.15 it stays hidden once its page or card has
been hidden.

Text the user sees is plain English. Don't use en or em dashes, arrows,
less-or-equal and greater-or-equal signs, or the ellipsis character. Commas are
fine. The `#` in stat lines stays, as the trade site uses it.

## Tests

Every piece of logic gets a QTest test. Put it in `tests/tst_<name>.cpp` and
add it to `tests/CMakeLists.txt` with the helper that fits, such as
`pou_add_test` or `pou_add_app_test`. The fakes in `tests/`, such as
`fakenetwork.h`, `fakegameinput.h` and `fakeopener.h`, stand in for the network,
the game, the browser and the clipboard, and the other things a test shouldn't
touch.

- Use made-up names and items in fixtures. Don't copy your own Path of Building
  builds into the tests, and don't commit them.
- Window tests run offscreen. `initMain` sets `QT_QPA_PLATFORM=offscreen`, and
  the tests call `QTest::failOnWarning()`.
- Tests must not touch the clipboard, open a page in the browser, map visible
  windows, send XTest input, register real global shortcuts or set off desktop
  prompts.
- Test fakes don't use `Q_OBJECT`.
- In a test, use normal strings rather than a raw string with an apostrophe or
  `//` in it, as moc then skips the file.
- Break the code on purpose and check the new test fails before you rely on it.

## Documentation

If your change alters what the app does, update the README in the same pull
request.

A change players would notice also gets a line under Unreleased in
[the changelog](../docs/CHANGELOG.md), in plain words: what it does for them,
not how it's built.

## License

By contributing, you agree that your work is released under the MIT license
in [LICENSE](../LICENSE).
