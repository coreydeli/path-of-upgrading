# Style guide

This is how Path of Upgrading writes its text, docs, code and commits. The
process for getting a change in (branches, pull requests, what CI checks) is in
[CONTRIBUTING.md](../.github/CONTRIBUTING.md), and the requirements and build steps
are in the [README](../README.md). Where this guide and the code disagree, the
code is right and this guide needs a fix.

## 1. Text the user sees

This covers every label, title, button, notification and message, in C++ and
in QML.

- Write plain English. Say what the app found or what it needs, in short
  sentences.
- Title Case for the window title, the sidebar entries, page titles, the
  section headers of a settings page, buttons and dialog titles:
  - `title: qsTr("Build Review")` on a page
  - `SectionHeader { title: qsTr("What Matters Most") }`
  - `text: qsTr("Open on the Trade Site")` and `text: qsTr("Show More")` on buttons
  - `title: qsTr("Add a Stat")` on a dialog
- Sentence case for switch and row labels, field labels, descriptions, status
  lines and messages:
  - `text: qsTr("Cap my resistances first")` on a switch
  - `description: qsTr("For your minions and auras.")` under a row
  - `u"Searching the trade site"_s` in a status line
- Table titles inside the popup are sentence case too, as in
  `title: qsTr("Stats that change")`.
- A sentence gets a full stop. Labels, buttons, headers and titles don't.
- Use the straight apostrophe, as in `qsTr("Path of Building wasn't found.")`.
- Start each message with a capital letter.
- Translatable text in QML goes in `qsTr()`, with numbers put in by `.arg()`:
  `qsTr("A Divine Orb is %1 Exalted Orbs in %2.").arg(...)`.
- Text in C++ is `u"..."_s` with `%1` placeholders filled by `.arg()`, as in
  `u"Couldn't write %1: %2"_s.arg(m_file, file.errorString())`.

### Errors

- Say what went wrong, and where it helps, what to do about it. Name the file
  or the thing that failed, and give the reason when there is one:
  - `u"Couldn't remove %1."_s.arg(m_file)`
  - `u"Path of Building's answer couldn't be read."_s`
  - `u"Searches need the trade site's list of stats, and it sent an empty one."_s`
- When the message can't say the fix, the control beside it can. On the Build
  Review page, `Path of Building wasn't found.` sits above the Update button,
  whose description says `Use it after you install Path of Building, or open
  it for Path of Exile 2 for the first time.`
- Log text is short and plain too. Pass a `QString` to `%s` through
  `qPrintable()`, as `ItemPopup::makeWindow` does.

### Numbers and units

- Use `formatNumber` (`src/core/numbers.h`) and the popup's `amount`
  (`src/app/itemreport.cpp`) rather than writing your own. `amount` shows whole
  numbers from 100 up, with commas from 1,000.
- Gains get a plus sign with no space, as in `+150` and `+4.2%`. Percent has no
  space before the sign, as in `Fire 75%`.
- Use the singular for one: `seconds == 1 ? u"Waiting a second for the trade site"_s : ...`
  and `capped.size() == 1 ? u"resistance"_s : u"resistances"_s`.
- Round a wait up to whole seconds, with `std::chrono::ceil`, so it never says
  0 while the wait is still running.
- Write currency in full in the app: "Divine Orbs", "Exalted Orbs", "Chaos
  Orbs". The docs may say "exalts" and "divines" in running text.

### Game terms

- Use the game's own names for stats, items and mods: Life, Energy Shield,
  Spirit, Effective HP, DPS, Minion DPS.
- Keep the `#` in stat lines, because the trade site uses it. A range is
  `"# to #"`, and the code tests for it with `row.key.contains(u"# to #")`.
- Element names start with a capital on a label, as `Fire Resistance`, and are lower
  case inside a sentence, as `Caps your fire resistance`.
- Keep the league names as the trade site has them, such as "Standard", and
  name the modes as the settings page does: "Hardcore", "Solo self-found".

### Forbidden characters

- No en dash, em dash, arrows, less-or-equal or greater-or-equal signs, or
  the single ellipsis character, in anything the user can see.
- Joining with a comma is fine: `u", "_s`.
- Text the parser reads from the game keeps the game's own characters. Don't
  change what the game prints.

## 2. Writing docs

This covers the README, CONTRIBUTING.md and anything else in `docs/`.

- Write to "you", in short sentences and plain words.
- Say what the app does, not what it is "designed to" do.
- Leave out marketing words: powerful, seamless, robust, blazing, simply, just.
- No emoji.
- Headings are in sentence case. Only proper names are capitalized, as in
  "Path of Building" and "Debian 13".
- Code, file names, settings keys and commands go in backticks, as the README
  does with `path-of-upgrading --background`.
- Shell commands go in a fenced ```` ```bash ```` block. A command the reader
  runs on its own gets its own block, as the Flatpak install does:

  ```bash
  flatpak install flathub community.pathofbuilding.PathOfBuilding
  ```

  Steps that run in order can share a block, as the build steps in the README do.
- Link, don't repeat. CONTRIBUTING.md points to the README for the requirements
  rather than copying them, and the README links within itself, as in
  [Requirements](../README.md#requirements).
- Use relative links to repo files. From `docs/` they start with `../`.
- A change to what the app does changes the README in the same pull request.

## 3. Code comments

Comments are short and plain. They explain what the code doesn't say: why
things happen in an order, what a caller has to know, or what a number stands
for. They don't narrate the next line, and they don't make up a reason.

Real comments that do the job:

```cpp
// the build goes first, so a failure leaves a character without its build
// rather than a build without its character
```

```cpp
// Whether what was fetched then is less than a day old. A time ahead of the
// clock counts as old, or it would never be asked for again.
```

```cpp
// which does nothing when it's the same
m_settings->set(settings);
```

```cpp
// past the cap only the cap counts
if (r.capped && r.wasCapped && change == 0)
```

Short ones work too: `// calls off what's still asked for` above a
`delete m_fetch;`, or `// how many figures a listing shows` above a constant.

- Put a comment above a header declaration when the caller needs to know it:
  `// The window the icon shows and hides. It has to outlive the tray.`
- Don't restate a name. An invented example of what to avoid:
  `// set the rate to the value`, above `setDivineRate(rate)`.
- Don't invent a reason. If you don't know why something is done, leave the
  comment out.
- A comment on the same line explains a value, as the code does:
  `QString exec; // the command, with whatever arguments it starts with`.

## 4. C++

### Files and namespace

- Every C++ file starts with `// SPDX-License-Identifier: MIT`. Headers then go
  on with `#pragma once`, and sources with their own header.
- Put the code in `namespace pou`, and close it with `} // namespace pou`.
  Anonymous helpers go in a `namespace { ... }` inside it.
- Group includes as `traderesults.cpp` does: the file's own header, then the
  project's headers in quotes, then Qt and the standard library in angle brackets.

### Formatting

- Run `clang-format -i` on the files you change. `.clang-format` sets the style:
  `ColumnLimit: 110`, 4-space indents, braces on their own line after classes,
  structs and functions, and `PointerAlignment: Right`.
- Put `*` and `&` next to the name, as in `const QString &name` and
  `QObject *parent`.

### Strings and logging

- Write text as `u"..."_s`, with `using namespace Qt::Literals::StringLiterals;`
  at the top of the file.
- Don't write `QString s = "x";`. The build turns off implicit conversions from
  plain C strings.
- Use `QStringView` for look-up keys that don't need to own their string, as in
  `StatMap::get(QStringView key)`.
- Use `u'x'` for a single character, as in `m_folder + u'/' + id`.
- Log through a category, such as `qCWarning(lcTrade, ...)`, `lcPob` or `lcInput`.

### Build settings

The build is compiled with these definitions, which stay on:

```
QT_NO_CAST_FROM_ASCII  QT_NO_CAST_TO_ASCII  QT_NO_CAST_FROM_BYTEARRAY
QT_NO_URL_CAST_FROM_STRING  QT_USE_QSTRINGBUILDER  QT_NO_FOREACH  QT_NO_KEYWORDS
```

- With `QT_NO_KEYWORDS`, write `Q_SIGNALS:`, `Q_SLOTS` and `Q_EMIT`, never
  `signals`, `slots` or `emit`. Use range-for, not `foreach`.
- The build has no warnings. CMake adds `-Wall -Wextra -Wpedantic` and more,
  and CI treats warnings as errors. Fix a warning rather than hide it.
- The code is C++20 for Qt 6.8. Don't use an API newer than Qt 6.8, KDE
  Frameworks 6.13 or Kirigami Addons 1.7.

### Naming

- Classes and structs are PascalCase: `TradeResults`, `ReportFigure`.
- Functions and variables are camelCase: `loadMore()`, `pobWeighedSlot()`.
- Data members start with `m_`: `m_listings`, `m_settings`, `m_fetch`.
- Constants are camelCase with no `k` prefix: `constexpr qsizetype figuresShown = 4;`.
- Enum values are PascalCase: `Verdict::Upgrade`, `Tone::Warning`. The enums in
  the core and platform code are `enum class`, as `Verdict` and
  `GameInput::Failure` are.
- Getters take the name of the thing, as `maximum()` and `margin()`, and the
  setter is `setMaximum(double)`.
- Predicates are questions: `isFromSheet()`, `isRefreshing()`.

### Declarations and initializers

- Use designated initializers in declaration order:
  `return {.label = label, .change = ..., .from = amount(c.from), .to = amount(c.to), .tone = toneOf(change)};`
- Put the commas at the start of each line in a constructor's initializer list,
  as `Resistances::Resistances` does.
- Use `std::optional` for a value that may be missing, and `std::nullopt` for none.
- Pass strings and other large values as `const` references, as the code does.

### Ownership and objects

- A function that makes an object returns a `std::unique_ptr`, as
  `createTray()`, `createNotifier()` and `createGameInput()` do.
- Pass a non-owned object as a raw pointer, and say in the comment who keeps it
  alive: `// They have to outlive it.`
- Hold a non-owned object that might go first in a `QPointer`, as
  `TrayNotifier` holds its `Tray`.
- A `QObject` with signals or slots has `Q_OBJECT`. A test fake does not.
- A call that can fail has `errorString()`, and a `bool` return says whether it
  worked, as `Autostart::setEnabled` does.

### QML value types and enums

- A value type that QML uses is a struct with `Q_GADGET`, `QML_VALUE_TYPE(name)`
  and `Q_PROPERTY(... MEMBER ...)`. The name is lower camel case, as
  `QML_VALUE_TYPE(reportFigure)`. Restate `public:` before the fields:

  ```cpp
  struct ReportFigure
  {
      Q_GADGET
      QML_VALUE_TYPE(reportFigure)

      Q_PROPERTY(QString label MEMBER label)
      Q_PROPERTY(pou::Tone::Tone tone MEMBER tone)

  public:
      QString label;  // "Life"
      Tone::Tone tone = Tone::Plain;
  };
  ```

- An enum for QML goes in a namespace with `Q_NAMESPACE` and `QML_ELEMENT`, then
  `Q_ENUM_NS`. `Tone` in `src/app/itemreport.h` is the example:

  ```cpp
  namespace Tone {
  Q_NAMESPACE
  QML_ELEMENT

  enum Tone {
      Plain,
      Good,
      Bad,
      Warning, // good, with a catch
  };
  Q_ENUM_NS(Tone)

  } // namespace Tone
  ```

### Platform code

- Anything specific to an operating system goes in `src/platform`. The rest of
  the code doesn't depend on it.
- Each platform has its own folder: `src/platform/xcb` for the X server through
  XWayland, `src/platform/kde` for the tray, `src/platform/win` for Windows.
- A file with both a Windows and a Linux branch uses `#ifdef Q_OS_WIN` and
  `#else`, as `src/platform/autostart.cpp` does.

## 5. QML

- Start every QML file with `// SPDX-License-Identifier: MIT`. Add
  `pragma ComponentBehavior: Bound` where delegates use `required property`, as
  `PopupCard.qml` does.
- Import Qt first, then the rest, in alphabetical order, with the usual aliases:

  ```qml
  import QtQuick
  import QtQuick.Controls as QQC2
  import QtQuick.Layouts
  import org.kde.kirigami as Kirigami
  import org.kde.kirigamiaddons.formcard as FormCard
  ```

- A page is a `ColumnsPage` with a `title`. `ColumnsPage.qml` is a
  `FormCard.FormCardPage` that flows its cards into up to three columns on a
  wide window, and stacks them on a narrow one. Its cards are `Sheet`s, the
  app's own cut-corner plate, and a `Sheet` with a title has a `SectionHeader`
  before it. Rows inside a `Sheet` are separated with `RowRule {}`. Set
  `minColumnWidth` and `maxColumnWidth` when a page's cards need more or less
  room. See `TradePage.qml`.
- Help text under a card is `SectionText`, which is as wide as the card it goes
  with. `FormSectionText` has a width limit, so it doesn't line up with a card.
- Use the app's own rows, not the Kirigami Addons ones:
  - `SwitchRow` for a switch, with `text`, `description`, `checked` and `onToggled`
  - `FormRow` for a row with controls on the right, such as the session ID field in `TradePage.qml`
  - `NumberField` for a number, with `label`, `value`, `decimals` and `noneText`
  - `FormCard.FormTextDelegate` for a line of status text
  - `FormCard.FormButtonDelegate` for an action
- Never put `trailing:` on a FormCard delegate. `FormRow.qml` explains why: a
  trailing item "stays hidden for good once its page or card has been hidden".
- The value comes from C++, and the handler writes it back:

  ```qml
  SwitchRow {
      objectName: "resistsSwitch"
      text: qsTr("Cap my resistances first")
      checked: pobPage.options.resistsFirst
      onToggled: pobPage.options.resistsFirst = checked
  }
  ```

  Use `SwitchRow`, not Kirigami Addons' `FormSwitchDelegate`: the delegate sets
  its own `checked`, which takes the binding off, as `SwitchRow.qml` notes.
- Dialogs are `FormCard.FormCardDialog`, as `StatPickerDialog.qml` is.
- Give an element a short camel-case `objectName` when a test has to find it,
  as `enabledSwitch`, `budgetMin` or `lookAgainButton`.
- Set `Accessible.name` on a control with no text. Mark the controls inside a
  row `Accessible.ignored: true` when the row already says what they do.
- Use freedesktop icon names for `icon.name`, as `"view-refresh"`.
- Comments are short, as in C++: `// the row has it`.

## 6. Tests

- Each piece of logic has a QTest test in `tests/tst_<piece>.cpp`, with a class
  named after the piece, as `TestAutostart`.
- Test functions are short camel-case phrases: `turnsOn`, `notHidden`, `noFile`,
  `failedWithASearch`.
- Add the test to `tests/CMakeLists.txt` with the helper that fits:
  `pou_add_test`, `pou_add_services_test`, `pou_add_platform_test`,
  `pou_add_app_test`, or `pou_add_qml_test` for a test that loads QML. A test
  that needs the Path of Building stand-in also calls `pou_use_fakepob`.
- Fakes stand in for what a test shouldn't touch, such as `FakeGameInput`,
  `FakeNetwork` and `FakeSecretStore`. A fake is a plain class with no `Q_OBJECT`,
  with a comment on what it stands in for:

  ```cpp
  // The game for tests, which copies nothing until the test says what it
  // copied, and counts what it was asked for.
  class FakeGameInput : public pou::GameInput
  ```

- Fixtures use made-up names and items, in headers such as `tradefixtures.h` and
  `itemtexts.h`. Never put your own Path of Building builds, or any real session
  ID, in a fixture or a commit.
- Window tests run offscreen. `initMain` sets `QT_QPA_PLATFORM=offscreen`, and
  the tests call `QTest::failOnWarning()`.
- A test must not touch the clipboard, open a page in the browser, map a visible
  window, send XTest input, register a real global shortcut or set off a desktop
  prompt.
- Use a normal string literal in a test, not a raw string, when the text has an
  apostrophe or `//` in it, as moc then skips the file.
- Break the code on purpose to check a new test fails, before you rely on it.
- Run the tests on the private bus the README describes.

## 7. Commits, branches and pull requests

- Make small commits, each one a single step, as a person would make them.
- Title each commit with a short imperative sentence that says what it does, in
  plain words. Real examples:
  - `Read the fake Path of Building's pipe unbuffered`
  - `Leave out the carriage returns in what Path of Building prints`
  - `Say how to install it on each system`
- Don't reuse a commit title, on this branch or any other.
- Commits and pull requests name only the people who made them, with no
  trailers or footers added by tools.
- Stage the paths you changed by name, and check the diff before you commit.
- Branch from `beta`. Name the branch `area/change`, as in `trade/signed-out`,
  `pob/timeouts` and `docs/install`.
- Merge `beta` into your branch when `beta` has moved on, as CONTRIBUTING.md says.
- Open the pull request into `beta`, and fill in
  [the template](../.github/PULL_REQUEST_TEMPLATE.md): what changed, how it was tested, and
  the checklist.
- Start each pull request title with its kind and a colon, then a short sentence:
  `Feature:` (label `enhancement`), `Bug:` (`bug`), `Docs:` (`documentation`),
  `CI:` (`ci`, for builds, tests and releases on GitHub Actions) or
  `Maintenance:` (`maintenance`, for revision passes and tidying with no change
  in behaviour). For example `Feature: Spread the settings pages across a wide
  window` and `Bug: Stop calling unrated stats not used`. Commit titles don't
  get the kind.
- Keep each pull request to one change.
- Delete the branch once it's merged, locally and on GitHub.

## 8. Names

- The app is **Path of Upgrading** in all text. The slug is `path-of-upgrading`,
  used for the binary, the settings file and the bus name. The app id is
  `io.github.coreydeli.PathOfUpgrading`, used for the desktop file name, the
  Wayland app_id and the icon name.
- Write the game and the tools out in full in user text: Path of Exile 2,
  Path of Building and the trade site. The Flatpak id is
  `community.pathofbuilding.PathOfBuilding`.
- Don't use any other name for the project, in code, comments, text, settings
  keys, file names or docs. The app has no migration code, and none is to be added.
