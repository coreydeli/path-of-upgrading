#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Gets the site's files ready for Zensical, in site/.work.

The site is the pages in site/content, plus the wiki, the README and the
changelog (read from where they are, never copied into the repository), plus
a downloads page made from the GitHub releases. Run it with -I:

    python3 -I site/build.py --wiki <wiki clone> [--releases <json file>]
    zensical build -f site/.work/zensical.toml

With no --releases it asks GitHub for them, with GITHUB_TOKEN when there is one.
"""
import argparse
import json
import os
import re
import shutil
import urllib.request
from datetime import datetime
from pathlib import Path

REPO = "coreydeli/path-of-upgrading"
SITE = "https://coreydeli.github.io/path-of-upgrading"
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent

# the files a release has, in the order the page lists them
KINDS = [
    # key, title, file name test, README heading of its install steps
    ("arch", "Arch Linux", lambda n: n.endswith(".pkg.tar.zst"), "Arch Linux"),
    ("debian", "Debian 13", lambda n: n.endswith(".deb"), "Debian 13"),
    ("fedora", "Fedora", lambda n: n.endswith(".rpm"), "Fedora"),
    ("appimage", "AppImage", lambda n: n.endswith(".AppImage"), "AppImage"),
    ("windows", "Windows", lambda n: n.endswith("-setup.exe") or n.endswith("-windows.zip"), "Windows"),
    ("source", "Other Linux distros", lambda n: n.endswith(".tar.gz"), "Other Linux distros"),
]


def read_sections(text, level):
    """Splits markdown into {heading: body} at headings of the given level.

    Headings inside code fences don't count.
    """
    marker = "#" * level + " "
    sections, name, body, fenced = {}, None, [], False
    for line in text.splitlines():
        if line.startswith("```"):
            fenced = not fenced
        if not fenced and line.startswith(marker):
            if name is not None:
                sections[name] = "\n".join(body).strip("\n")
            name, body = line[len(marker):].strip(), []
        elif name is not None:
            body.append(line)
    if name is not None:
        sections[name] = "\n".join(body).strip("\n")
    return sections


def demote(text, by):
    """Moves the headings of a piece of markdown down, outside code fences."""
    out, fenced = [], False
    for line in text.splitlines():
        if line.startswith("```"):
            fenced = not fenced
        if not fenced and re.match(r"#{1,5} ", line):
            line = "#" * by + line
        out.append(line)
    return "\n".join(out)


def fix_repo_links(text, folder=""):
    """Points links to files in the repository at GitHub."""

    def link(m):
        target = m.group(2)
        if re.match(r"[a-z]+:", target):
            return m.group(0)
        if target.startswith("#"):
            return f"{m.group(1)}(https://github.com/{REPO}/blob/main/README.md{target})"
        return f"{m.group(1)}(https://github.com/{REPO}/blob/main/{folder}{target})"

    return re.sub(r"(\[[^\]]*\])\(([^)\s]+)\)", link, text)


def wiki_links(text, pages):
    """Turns the wiki's links to its own pages into links between the files."""

    def target(name):
        name = name.strip().replace(" ", "-")
        page, _, anchor = name.partition("#")
        if page not in pages:
            return None
        return ("index" if page == "Home" else page) + ".md" + ("#" + anchor if anchor else "")

    # [[Page]] and [[Words|Page]]
    def wikilink(m):
        words, _, page = m.group(1).partition("|")
        page = page or words
        t = target(page)
        return f"[{words.strip()}]({t})" if t else words

    text = re.sub(r"\[\[([^\]]+)\]\]", wikilink, text)

    # [Words](Page) and [Words](Page#anchor)
    def mdlink(m):
        t = target(m.group(2))
        return f"{m.group(1)}({t})" if t else m.group(0)

    return re.sub(r"(\[[^\]]*\])\(([^)\s:]+)\)", mdlink, text)


def fetch_json(url):
    request = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json"})
    token = os.environ.get("GITHUB_TOKEN")
    if token and url.startswith("https://api.github.com/"):
        request.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(request, timeout=60) as reply:
        return json.load(reply)


def fetch_text(url):
    with urllib.request.urlopen(url, timeout=60) as reply:
        return reply.read().decode()


def read_sums(release):
    """Reads a release's SHA256SUMS into {file name: checksum}."""
    for asset in release["assets"]:
        if asset["name"] == "SHA256SUMS":
            sums = {}
            for line in fetch_text(asset["browser_download_url"]).splitlines():
                parts = line.split()
                if len(parts) == 2:
                    sums[parts[1].lstrip("*")] = parts[0]
            return sums
    return {}


def size(n):
    return f"{n / 1_000_000:.1f} MB"


def day(stamp):
    return datetime.fromisoformat(stamp.replace("Z", "+00:00")).strftime("%Y-%m-%d")


def pick(releases):
    """The latest release, and the newest beta if it's newer than that."""
    live = [r for r in releases if not r["draft"]]
    stable = next((r for r in live if not r["prerelease"]), None)
    beta = next((r for r in live if r["prerelease"]), None)
    if beta and stable and beta["published_at"] <= stable["published_at"]:
        beta = None
    return stable, beta


def files_table(release, kind, sums):
    rows = []
    for asset in release["assets"]:
        if kind[2](asset["name"]):
            rows.append(
                f"| [{asset['name']}]({asset['browser_download_url']}) | {size(asset['size'])} "
                f"| `{sums.get(asset['name'], 'not listed')}` |"
            )
    if not rows:
        return ""
    return "| File | Size | SHA-256 |\n| --- | --- | --- |\n" + "\n".join(rows) + "\n"


def downloads_page(releases, readme):
    stable, beta = pick(releases)
    steps = read_sections(readme["Installing"], 3) if "Installing" in readme else {}
    out = [
        "---",
        "title: Downloads",
        "hide:",
        "  - toc",
        "---",
        "",
        "# Downloads",
        "",
        "Pick the file for your system, install it as the steps say, and you're done. "
        "The links go to GitHub, where the files are kept.",
        "",
    ]
    for label, release in (("Release", stable), ("Beta", beta)):
        if release:
            out.append(
                f"**{label}:** [{release['name']}]({release['html_url']}), "
                f"out on {day(release['published_at'])}."
            )
        elif label == "Beta":
            out.append(
                "**Beta:** there isn't one newer than the release right now. "
                "Betas come out first, with the changes the next release will have."
            )
        else:
            out.append("**Release:** there isn't one yet.")
        out.append("")
    out += [
        "Betas are for anyone who wants the newest changes early, and they can be rough. "
        "[Releases and Branches](docs/Releases-and-Branches.md) says how they're made.",
        "",
        f"The list was made from the [releases page](https://github.com/{REPO}/releases) "
        f"on {datetime.now().astimezone().strftime('%Y-%m-%d')}, and the site updates whenever "
        "a release comes out. If a link is missing, the releases page is always current.",
        "",
    ]
    sums = {}
    for release in (stable, beta):
        if release:
            sums[release["tag_name"]] = read_sums(release)
    for kind in KINDS:
        out += [f"## {kind[1]}", ""]
        shown = False
        for label, release in (("Release", stable), ("Beta", beta)):
            if not release:
                continue
            table = files_table(release, kind, sums[release["tag_name"]])
            if table:
                out += [f"**{label}**", "", table]
                shown = True
        if not shown:
            out += ["Not in the latest release.", ""]
        body = steps.get(kind[3])
        if body:
            out += ["### Installing", "", fix_repo_links(demote(body, 1)), ""]
    for name in ("Checking a download", "Path of Building", "Uninstalling"):
        if name in steps:
            out += [f"## {name}", "", fix_repo_links(steps[name]), ""]
    return "\n".join(out)


def nav_from_sidebar(sidebar, pages):
    """Reads the wiki's sidebar into (group, [(title, file)]) pairs."""
    groups, group = [], None
    for line in sidebar.splitlines():
        if m := re.match(r"\*\*(.+)\*\*$", line.strip()):
            group = (m.group(1), [])
            groups.append(group)
        elif m := re.match(r"- \[([^\]]+)\]\(([^)]+)\)", line.strip()):
            page = m.group(2)
            if group and page in pages and page != "Home":
                group[1].append((m.group(1), page))
    return [g for g in groups if g[1]]


def toml_str(text):
    return json.dumps(text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--wiki", required=True, help="a clone of the wiki")
    parser.add_argument("--releases", help="a JSON file of the releases, instead of asking GitHub")
    parser.add_argument("--out", default=str(HERE / ".work"))
    args = parser.parse_args()

    wiki = Path(args.wiki)
    work = Path(args.out)
    if work.exists():
        shutil.rmtree(work)
    docs = work / "content"
    shutil.copytree(HERE / "content", docs)

    # fonts and the icon come from the app's own files
    fonts = docs / "assets" / "fonts"
    fonts.mkdir(parents=True)
    for name in ("Spectral-Regular", "Spectral-Italic", "Spectral-SemiBold", "BigShouldersDisplay-Bold"):
        shutil.copy(ROOT / "data" / "fonts" / f"{name}.ttf", fonts)
    for name in ("OFL-Spectral", "OFL-BigShoulders"):
        shutil.copy(ROOT / "data" / "fonts" / f"{name}.txt", fonts)
    shutil.copy(ROOT / "data" / "icons" / "io.github.coreydeli.PathOfUpgrading.svg", docs / "assets" / "icon.svg")

    # the wiki's pages, as the docs
    pages = {p.stem for p in wiki.glob("*.md") if not p.name.startswith("_")}
    (docs / "docs").mkdir()
    for page in pages:
        text = wiki_links((wiki / f"{page}.md").read_text(), pages)
        (docs / "docs" / ("index.md" if page == "Home" else f"{page}.md")).write_text(text)
    groups = nav_from_sidebar((wiki / "_Sidebar.md").read_text(), pages)

    # the parts of the README that fit, and the changelog
    readme_text = (ROOT / "README.md").read_text()
    readme = read_sections(readme_text, 2)
    about = {
        "Features": ("features.md", "Features", ["Features"]),
        "Requirements": ("requirements.md", "Requirements", ["Requirements"]),
        "Credits": ("credits.md", "Credits and License", ["Credits", "License"]),
    }
    for key, (file, title, names) in about.items():
        body = "\n\n".join(f"## {n}\n\n{readme[n]}" if len(names) > 1 else readme[n] for n in names)
        body = fix_repo_links(body)
        (docs / "docs" / file).write_text(f"# {title}\n\n{body}\n")
    changelog = (ROOT / "docs" / "CHANGELOG.md").read_text()
    (docs / "docs" / "changelog.md").write_text(fix_repo_links(changelog, "docs/"))
    # The license page is about the fonts too, so it carries their licenses.
    (docs / "docs" / "credits.md").write_text(
        (docs / "docs" / "credits.md").read_text()
        + "\nThe full licenses of the fonts are here: "
        "[Spectral](../assets/fonts/OFL-Spectral.txt) and "
        "[Big Shoulders](../assets/fonts/OFL-BigShoulders.txt).\n"
    )

    # the downloads page
    if args.releases:
        releases = json.loads(Path(args.releases).read_text())
    else:
        releases = fetch_json(f"https://api.github.com/repos/{REPO}/releases?per_page=30")
    (docs / "downloads.md").write_text(downloads_page(releases, {"Installing": readme["Installing"]}))

    # the navigation, one top level entry to a line as TOML wants it
    def entry(title, target):
        if isinstance(target, str):
            return "{ %s = %s }" % (toml_str(title), toml_str(target))
        return "{ %s = [%s] }" % (toml_str(title), ", ".join(entry(t, x) for t, x in target))

    docs_nav = [("Start here", "docs/index.md")]
    docs_nav += [(title, [(t, f"docs/{p}.md") for t, p in group]) for title, group in groups]
    nav = [
        ("Home", "index.md"),
        ("Downloads", "downloads.md"),
        ("Docs", docs_nav),
        (
            "About",
            [
                ("Features", "docs/features.md"),
                ("Requirements", "docs/requirements.md"),
                ("Changelog", "docs/changelog.md"),
                ("Credits and License", "docs/credits.md"),
            ],
        ),
    ]
    nav_text = "nav = [\n" + "".join(f"  {entry(t, x)},\n" for t, x in nav) + "]"
    config = (HERE / "zensical.toml").read_text()
    config = re.sub(r"^nav = \[\].*$", lambda _: nav_text, config, flags=re.M)
    (work / "zensical.toml").write_text(config)
    print(f"Staged the site in {work}")


if __name__ == "__main__":
    main()
