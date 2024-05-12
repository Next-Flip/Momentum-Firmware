#!/usr/bin/env python
import datetime as dt
import requests
import json
import sys
import os


if __name__ == "__main__":
    with open(os.environ["GITHUB_EVENT_PATH"], "r") as f:
        event = json.load(f)

    webhook = "DEV_WEBHOOK"
    title = desc = url = ""
    color = 0
    fields = []

    match os.environ["GITHUB_EVENT_NAME"]:
        case "push":
            webhook = "BUILD_WEBHOOK"
            count = len(event["commits"])
            if count == 20:
                count = int(requests.get(
                    event["compare"].replace("github.com", "api.github.com/repos"),
                    headers={
                        "Accept": "application/vnd.github.v3+json",
                        "Authorization": f"token {os.environ['GITHUB_TOKEN']}"
                    }
                ).json()["total_commits"])
            branch = event["ref"].removeprefix("refs/heads/")
            change = (
                "Force Push"
                if event["forced"] and not count
                else f"{count} New Commit{'' if count == 1 else 's'}"
            )
            desc = f"[**{change}**]({event['compare']}) | [{branch}]({event['repository']['html_url']}/tree/{branch})\n"
            for i, commit in enumerate(event["commits"]):
                msg = commit['message'].splitlines()[0].replace("`", "").replace("_", "\_")
                msg = msg[:50] + ("..." if len(msg) > 50 else "")
                desc += f"\n[`{commit['id'][:8]}`]({commit['url']}): {msg} - [__{commit['author'].get('username')}__](https://github.com/{commit['author'].get('username')})"
                if len(desc) > 2020:
                    desc = desc.rsplit("\n", 1)[0] + f"\n+ {count - i} more commits"
                    break
            url = event["compare"]
            color = 16723712 if event["forced"] else 11761899

        case "release":
            webhook = "RELEASE_WEBHOOK"
            color = 9471191
            version_tag = event['release']['tag_name']
            title = f"New Release: `{version_tag}`!"
            desc += f"> 💻 [**Web Installer**](https://momentum-fw.dev/update)\n\n"
            desc += f"> 🐬 [**Changelog & Download**](https://github.com/Next-Flip/Momentum-Firmware/releases/tag/{version_tag})\n\n"
            desc += f"> 🛞 [**Project Page**](https://github.com/Next-Flip/Momentum-Firmware)"

        case "workflow_run":
            run = event["workflow_run"]
            url = run["html_url"]
            title = "Workflow "
            match run["conclusion"]:
                case "action_required":
                    title += "Requires Attention"
                    color = 16751872
                case "failure":
                    title += "Failed"
                    color = 16723712
                case _:
                    sys.exit(0)
            title += f": {run['name']}"

        case "issues":
            issue = event["issue"]
            url = issue["html_url"]
            name = issue["title"][:50] + ("..." if len(issue["title"]) > 50 else "")
            title = f"Issue {event['action'].title()}: {name}"
            match event["action"]:
                case "opened":
                    desc = (issue["body"][:2045] + "...") if len(issue["body"]) > 2048 else issue["body"]
                    color = 3669797
                case "closed":
                    color = 16723712
                case "reopened":
                    color = 16751872
                case _:
                    sys.exit(1)

        case "issue_comment":
            comment = event["comment"]
            issue = event["issue"]
            url = comment["html_url"]
            title = f"New Comment on Issue: {issue['title']}"
            color = 3669797
            desc = (comment["body"][:2045] + "...") if len(comment["body"]) > 2048 else comment["body"]

        case _:
            sys.exit(1)

    requests.post(
        os.environ[webhook],
        headers={"Accept": "application/json", "Content-Type": "application/json"},
        json={
            "content": None,
            "embeds": [
                {
                    "title": title[:256],
                    "description": desc[:2048],
                    "url": url,
                    "color": color,
                    "fields": fields[:25],
                    "author": {
                        "name": event["sender"]["login"][:256],
                        "url": event["sender"]["html_url"],
                        "icon_url": event["sender"]["avatar_url"],
                    },
                    "timestamp": dt.datetime.utcnow().isoformat()
                }
            ],
            "attachments": [],
        },
    )
