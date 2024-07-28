#!/usr/bin/env python
import datetime as dt
import subprocess
import requests
import json
import re
import os

artifact_tgz = f"{os.environ['INDEXER_URL']}/firmware/dev/{os.environ['ARTIFACT_TAG']}.tgz"
artifact_sdk = f"{os.environ['INDEXER_URL']}/firmware/dev/{os.environ['ARTIFACT_TAG'].replace('update', 'sdk')}.zip"
artifact_lab = f"https://lab.flipper.net/?url={artifact_tgz}&channel=dev-cfw&version={os.environ['VERSION_TAG']}"


def parse_diff(diff: str):
    lines = diff.splitlines()[5:]
    parsed = {}
    previndent = ""
    prevtext = ""
    categories = []

    for line in lines:
        text = line[1:]
        if not text.strip():
            continue

        indent = re.match("^[-# ]+", text)
        if not indent:
            continue
        indent = indent[0].rstrip()
        if indent.startswith("#"):
            categories = []
            indent = ""
        elif indent != previndent:
            if len(indent) > len(previndent):
                categories.append(prevtext)
            else:
                categories.pop()
        previndent = indent
        prevtext = text

        change = line[0]
        if change != " ":
            section = parsed
            for category in categories:
                section.setdefault(category, {})
                section = section[category]
            section.setdefault(text, {})
            section[text][None] = change

    return parsed


def format_changes(section, name=None, lines=None):
    if not name:
        lines = []
    else:
        if name.startswith("#"):
            lines.append(f"\n{name}")
        else:
            change = section.get(None)
            name = name.replace("-", change or "*", 1)
            if change:
                name = change + name[1:]
            lines.append(name)

    for key, value in section.items():
        if key is not None:
            format_changes(value, name=key, lines=lines)

    if not name:
        return "\n".join(lines).lstrip()


if __name__ == "__main__":
    with open(os.environ["GITHUB_EVENT_PATH"], "r") as f:
        event = json.load(f)

    release = "release"
    before = event["before"]
    after = event["after"]
    compare = event["compare"].rsplit("/", 1)[0]

    # Saved before uploading new devbuild
    with open("previndex.json", "r") as f:
        previndex = json.load(f)
    for channel in previndex["channels"]:
        if channel["id"] == "release":
            release = channel["versions"][0]["version"]
        if channel["id"] == "development":
            before = channel["versions"][0]["version"]

    last_build_diff = subprocess.check_output(["git", "diff", f"{before}:CHANGELOG.md", f"{after}:CHANGELOG.md", "-U99999"]).decode()
    parsed = parse_diff(last_build_diff)
    changes = format_changes(parsed)

    requests.post(
        os.environ["BUILD_WEBHOOK"],
        headers={"Accept": "application/json", "Content-Type": "application/json"},
        json={
            "content": None,
            "embeds": [
                {
                    "title": f"New Devbuild: `{os.environ['VERSION_TAG']}`!",
                    "description": "",
                    "url": "",
                    "color": 16751147,
                    "fields": [
                        {
                            "name": "Code Diff:",
                            "value": "\n".join([
                                f"[From last release ({release} to {after[:8]})]({compare}/{release}...{after})",
                                f"[From last build ({before[:8]} to {after[:8]})]({compare}/{before}...{after})",
                            ])
                        },
                        {
                            "name": "Changelog:",
                            "value": "\n".join([
                                f"[Since last release ({release})]({event['repository']['html_url']}/blob/{after}/CHANGELOG.md)",
                                f"Since last build ({before[:8]}):",
                                "```diff",
                                changes,
                                "```",
                            ])
                        },
                        {
                            "name": "Firmware Artifacts:",
                            "value": "\n".join([
                                f"- [🖥️ Install with Web Updater](https://momentum-fw.dev/update)",
                                f"- [☁️ Open in Flipper Lab/App]({artifact_lab})",
                                f"- [🐬 Download Firmware TGZ]({artifact_tgz})",
                                f"- [🛠️ SDK (for development)]({artifact_sdk})",
                            ])
                        }
                    ],
                    "timestamp": dt.datetime.utcnow().isoformat()
                }
            ],
        },
    )
