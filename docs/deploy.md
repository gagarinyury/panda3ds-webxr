# Deploying the web player (panda.profy.top)

Live: **https://panda.profy.top/** (Cloudflare Pages project `panda-profy`). It is only the player page, the emulator core and three free homebrew demos; no game files are ever uploaded, people open their own.

## Build and deploy

```sh
sh scripts/make-pages.sh        # builds dist/: index.html at the root, build-core/, run/examples (3 demos), icon.svg, og.png, robots.txt, sitemap.xml, _headers
# Requires CLOUDFLARE_API_TOKEN and CLOUDFLARE_ACCOUNT_ID in the environment
cd /tmp && npx --yes wrangler@4 pages deploy <repo>/dist --project-name panda-profy --branch main --commit-dirty=true
```

Notes learnt the hard way:
- Run `wrangler` from a clean directory, not from the repo root: the repo has a `config.toml` and wrangler 4 then tries to auto-detect a framework (Hugo, Zola) and fails. A new project needs `--force` once (`pages project create panda-profy --production-branch main --force`); after that never pass `--force`.
- Cloudflare Pages redirects `/x/index.html` to `/x/` (308), so the page lives at `dist/index.html` and `make-pages.sh` rewrites the core path `../build-core/` to `/build-core/`.
- First setup only: `POST /accounts/<id>/pages/projects/panda-profy/domains {"name":"panda.profy.top"}` and a proxied CNAME `panda` -> `panda-profy.pages.dev` in the zone.

## Search

The page carries a title, description, canonical, Open Graph / Twitter tags (`og.png`, made with PIL), JSON-LD `SoftwareApplication`; `robots.txt` allows everything and lists `sitemap.xml`. Search Console: `sc-domain:profy.top` covers the subdomain, only the sitemap `https://panda.profy.top/sitemap.xml` has to be submitted.

## Local copy for showing something before it is deployed

`sh ios/prepare-site.sh` (updates `ios/Site` in place), `python3 scripts/serve-site.py 8791` (no-store headers) and, if a public link is needed for a few hours, `cloudflared tunnel --url http://127.0.0.1:8791`. The tunnel address dies when the processes stop.
