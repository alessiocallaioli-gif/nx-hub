# Legge registry/packages.json, interroga GitHub e scrive registry/resolved.json
# con ultima versione e URL di download gia' risolti (la Switch fa 1 sola richiesta).
import json, fnmatch, os, urllib.request
tok = os.environ.get("GITHUB_TOKEN")
def get(url):
    h = {"User-Agent": "nx-hub", "Accept": "application/vnd.github+json"}
    if tok: h["Authorization"] = "Bearer " + tok
    return json.load(urllib.request.urlopen(urllib.request.Request(url, headers=h), timeout=30))
data = json.load(open("registry/packages.json"))
for p in data["packages"]:
    p.pop("latest", None); p.pop("url", None)
    if not p.get("repo"): continue
    try:
        rel = get("https://api.github.com/repos/%s/releases/latest" % p["repo"])
        for a in rel["assets"]:
            if fnmatch.fnmatch(a["name"], p.get("asset_pattern", "*.zip")):
                p["latest"] = rel["tag_name"]; p["url"] = a["browser_download_url"]; break
        else:
            print("nessun asset per", p["id"])
    except Exception as e:
        print("saltato", p["id"], e)
json.dump(data, open("registry/resolved.json", "w"), indent=2)
