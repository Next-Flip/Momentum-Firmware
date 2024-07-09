#!/usr/bin/env python
import requests
import json
import os

if __name__ == "__main__":
    with open(os.environ["GITHUB_EVENT_PATH"], "r") as f:
        event = json.load(f)
    release = requests.get(
        event["release"]["url"],
        headers={
            "Accept": "application/vnd.github.v3+json",
            "Authorization": f"token {os.environ['GITHUB_TOKEN']}"
        }
    ).json()
    version_tag = release["tag_name"]

    with open("CHANGELOG.md", "r") as f:
        changelog = f.read()

    notes_path = '.github/workflow_data/release.md'
    with open(notes_path, "r") as f:
        template = f.read()
    notes = template.format(
        VERSION_TAG=version_tag,
        CHANGELOG=changelog
    )
    with open(notes_path, "w") as f:
        f.write(notes)
