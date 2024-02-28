#!/bin/bash

export VERSION_TAG="$(python -c '''
import datetime as dt
import json
import os
with open(os.environ["GITHUB_EVENT_PATH"], "r") as f:
    event = json.load(f)
version = int(event["pull_request"]["title"].removeprefix("V").removesuffix(" Release")
date = dt.datetime.now().strftime("%d%m%Y")
print(f"MNTM-{version:03}_{date}", end="")
''')"
echo "VERSION_TAG=${VERSION_TAG}" >> $GITHUB_ENV
