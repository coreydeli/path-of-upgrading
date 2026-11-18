# The website

The site at https://getpou.app/ is made here and published by
`.github/workflows/pages.yml`. You don't build it by hand to change it, but
you can.

## How it's made

- **GitHub Pages** hosts plain files for free. A workflow builds the files and
  hands them to Pages. Pages is on for this repository (Source: GitHub
  Actions), and its `github-pages` environment only allows deploys from `main`.
- **The address** is `getpou.app`, set in the repository's Pages settings. Its
  DNS at Cloudflare points straight at GitHub Pages (not proxied), so GitHub
  makes its certificate. `pathofupgrading.app` is a Cloudflare redirect rule
  that sends every page to the same path on `getpou.app`. A site deployed by
  a workflow needs no `CNAME` file.
- **Zensical** (a Python tool, pinned in `requirements.txt`) turns markdown
  into the pages, with search that runs in the visitor's browser. Nothing is
  tracked, no fonts or scripts come from other sites, and the header doesn't
  ask GitHub for anything.
- **`build.py`** gathers the pages into `site/.work` first:
  - `content/` is the home page and the look (`assets/site.css`).
  - The docs are the wiki's pages, cloned fresh each build. The wiki stays the
    source of truth. Its links to other pages, `[Words](Page)` and
    `[[Page]]`, become links between the site's pages, and the order comes
    from the wiki's `_Sidebar.md`.
  - Features, Requirements and Credits come from the README, and the
    changelog from `docs/CHANGELOG.md`.
  - The downloads page comes from the GitHub releases: the latest release,
    and the newest beta when it's newer, with sizes, checksums from each
    `SHA256SUMS`, and the install steps from the README's Installing section.
  - The fonts and icon are copied from `data/`.

## When it's rebuilt

A release made with the workflow's own token can't start another workflow, so
the site is rebuilt when the Release workflow finishes on a push to main or a
beta run, each morning (which catches wiki edits), and by hand (Actions,
Website, Run workflow). A pull request that changes `site/` only builds it.

## Building it here

```bash
git clone https://github.com/coreydeli/path-of-upgrading.wiki.git /some/folder/wiki
python3 -m venv /some/folder/venv
/some/folder/venv/bin/pip install -r site/requirements.txt
python3 -I site/build.py --wiki /some/folder/wiki
/some/folder/venv/bin/zensical build --config-file site/.work/zensical.toml
python3 -I -m http.server --directory site/.work/site
```

## Screenshots

There are none yet. Put images in `content/assets/` and add them to
`content/index.md` where the comment says.
