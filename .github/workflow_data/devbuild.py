#!/usr/bin/env python
import datetime as dt
import requests
import json
import os

artifact_tgz = f"{os.environ['INDEXER_URL']}/firmware/dev/{os.environ['ARTIFACT_TAG']}.tgz"
artifact_sdk = f"{os.environ['INDEXER_URL']}/firmware/dev/{os.environ['ARTIFACT_TAG'].replace('update', 'sdk')}.zip"


if __name__ == "__main__":
    with open(os.environ["GITHUB_EVENT_PATH"], "r") as f:
        event = json.load(f)

    release = "release"
    before = event["before"][:8]
    after = event["after"][:8]
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
                            "name": "Diff since last build:",
                            "value": f"[Compare {before} to {after}]({compare}/{before}...{after})"
                        },
                        {
                            "name": "Diff since last release:",
                            "value": f"[Compare {release} to {after}]({compare}/{release}...{after})"
                        },
                        {
                            "name": "Changelog since last release:",
                            "value": f"[Changes since {release}]({event['repository']['html_url']}/blob/{after}/ChangeLog.md)"
                        },
                        {
                            "name": "Download artifacts:",
                            "value": f"- [Download Firmware TGZ]({artifact_tgz})\n- [SDK (for development)]({artifact_sdk})"
                        }
                    ],
                    "timestamp": dt.datetime.now(dt.UTC).isoformat()
                }
            ],
        },
    )
