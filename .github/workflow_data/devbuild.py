#!/usr/bin/env python
import datetime as dt
import requests
import json
import os

base_url = f"{os.environ['INDEXER_URL']}/builds/firmware/dev"
artifact_tgz = f"{base_url}/{os.environ['ARTIFACT_TAG']}.tgz"
artifact_sdk = f"{base_url}/{os.environ['ARTIFACT_TAG'].replace('update', 'sdk')}.zip"
artifact_lab = f"https://lab.flipper.net/?url={artifact_tgz}&channel=dev-cfw&version={os.environ['VERSION_TAG']}"


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
                            "value": "\n".join(
                                [
                                    f"[From last release ({release} to {after[:8]})]({compare}/{release}...{after})",
                                    f"[From last build ({before[:8]} to {after[:8]})]({compare}/{before}...{after})",
                                ]
                            ),
                        },
                        {
                            "name": "Changelog:",
                            "value": "\n".join(
                                [
                                    f"[Since last release ({release})]({event['repository']['html_url']}/blob/{after}/CHANGELOG.md)",
                                ]
                            ),
                        },
                        {
                            "name": "Firmware Artifacts:",
                            "value": "\n".join(
                                [
                                    f"- [🖥️ Install with Web Updater](https://momentum-fw.dev/update)",
                                    f"- [☁️ Open in Flipper Lab/App]({artifact_lab})",
                                    f"- [🐬 Download Firmware TGZ]({artifact_tgz})",
                                    f"- [🛠️ SDK (for development)]({artifact_sdk})",
                                ]
                            ),
                        },
                    ],
                    "timestamp": dt.datetime.utcnow().isoformat(),
                }
            ],
        },
    )
