import nextcloud_client
import requests
import json
import os

if __name__ == "__main__":
    client = nextcloud_client.Client(os.environ["NC_HOST"])
    client.login(os.environ["NC_USER"], os.environ["NC_PASS"])

    file = os.environ["ARTIFACT_TGZ"]
    path = f"MNTM-Release/{file}"
    try:
        client.delete(path)
    except Exception:
        pass
    client.put_file(path, file)

    file = file.removesuffix(".tgz") + ".md"
    path = path.removesuffix(".tgz") + ".md"
    try:
        client.delete(path)
    except Exception:
        pass
    client.put_file(path, file)

    version = os.environ['VERSION_TAG'].split("_")[0]
    files = (
        os.environ['ARTIFACT_TGZ'],
        os.environ['ARTIFACT_TGZ'].removesuffix(".tgz") + ".md"
    )
    for file in client.list("MNTM-Release"):
        if file.name.startswith(version) and file.name not in files:
            try:
                client.delete(file.path)
            except Exception:
                pass
