#!/bin/sh
# Builds dist/ for a static host (Cloudflare Pages): the player page, the emulator core and three free homebrew demos. No game images.
# Deploy: wrangler pages deploy dist --project-name <name>   (see docs/deploy.md)
set -e
cd "$(dirname "$0")/.."
rm -rf dist && mkdir -p dist/build-core dist/run/examples
sed 's|\.\./build-core/|/build-core/|g' web/index.html > dist/index.html   # the page sits at the root of the site
cp build-core/AlberWeb.js build-core/AlberWeb.wasm dist/build-core/
for d in graphics__gpu__textured_cube graphics__gpu__toon_shading graphics__gpu__fragment_light; do cp run/examples/$d.3dsx dist/run/examples/; done
cp run/ctr-hello.3dsx dist/run/
cp web/icon.svg web/og.png web/apple-touch-icon.png web/icon-192.png web/icon-512.png web/manifest.webmanifest dist/
cat > dist/_headers <<'H'
/*
  X-Content-Type-Options: nosniff
/
  Cache-Control: no-cache
/index.html
  Cache-Control: no-cache
/manifest.webmanifest
  Content-Type: application/manifest+json
/build-core/*
  Cache-Control: public, max-age=3600, must-revalidate
H
cat > dist/robots.txt <<'R'
User-agent: *
Allow: /

Sitemap: https://panda.profy.top/sitemap.xml
R
cat > dist/sitemap.xml <<'R'
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9"><url><loc>https://panda.profy.top/</loc><changefreq>weekly</changefreq><priority>1.0</priority></url></urlset>
R
du -sh dist; find dist -type f | wc -l
