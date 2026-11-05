## What changed

<!-- What this does and why, in a line or two. Link the issue it closes, if there is one. -->

### Features

<!-- What the app can do now that it couldn't before. Leave out the heading if there's nothing. -->

### Fixes

<!-- What was wrong, and what it does now. Leave out the heading if there's nothing. -->

### Other changes

<!-- Tests, refactoring, CI, docs and anything else users won't notice. Leave out the heading if there's nothing. -->

## How it was tested

<!-- The tests you ran, and anything you checked by hand in the app. Say what you couldn't try. -->

## Checklist

- [ ] The pull request is into `beta`
- [ ] The title starts with its kind (`Feature:`, `Bug:`, `Docs:`, `CI:` or `Maintenance:`), and the label matches (`enhancement`, `bug`, `documentation`, `ci` or `maintenance`), so the release notes group it
- [ ] It builds with no warnings
- [ ] `clang-format` has run on the files I changed
- [ ] `ctest` passes on the private bus, as the README says
- [ ] New logic has a test, and I broke the code on purpose to check it fails
- [ ] Each commit title is new and says what the commit does
- [ ] Text the user sees is plain English, with no dashes or arrows
- [ ] The README is updated if what the app does has changed
- [ ] Nothing private is in the change: no session ID, real builds or personal data
