# TyrianGbaPoc official website

This directory contains the static project website. It has no package-manager
or external runtime dependency.

The public site is deployed to:

```text
https://erspicu.github.io/TyrianGbaPoc/
```

Every page supports English and Traditional Chinese through the shared
`assets/js/i18n.js` catalog. English is used on first visit; a language change
is stored in `localStorage` and follows the reader across all site pages.

On Windows, double-click `Serve-Website.bat`, then open:

```text
http://127.0.0.1:8080/
```

Alternatively, run any static HTTP server from this directory. Opening the
HTML files directly also works, but a local server is preferred for consistent
relative-link and browser-security behaviour.

The website intentionally links ROM downloads to GitHub Releases. ROM files
are release artifacts and must not be committed into this directory.

`.github/workflows/deploy-pages.yml` publishes this directory as the official
GitHub Pages site. A push to `main` that changes `Website/**` automatically
deploys a new version; `workflow_dispatch` also permits a manual redeploy.
