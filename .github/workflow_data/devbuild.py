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
                            "name": "Changes since last commit:",
                            "value": f"[Compare {event['before'][:7]} to {event['after'][:7]}]({event['compare']})"
                        },
                        {
                            "name": "Changes since last release:",
                            "value": f"[Compare release to {event['after'][:7]}]({event['compare'].rsplit('/', 1)[0] + '/release...' + event['after']})"
                        },
                        {
                            "name": "Download artifacts:",
                            "value": f"- [Download Firmware TGZ]({artifact_tgz})\n- [SDK (for development)]({artifact_sdk})"
                        }
                    ],
                    "timestamp": dt.datetime.utcnow().isoformat()
                }
            ],
        },
    )
