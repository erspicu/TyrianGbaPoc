# TyrianGbaPoc local website

This directory contains the static project website. It has no package-manager
or external runtime dependency.

On Windows, double-click `Serve-Website.bat`, then open:

```text
http://127.0.0.1:8080/
```

Alternatively, run any static HTTP server from this directory. Opening the
HTML files directly also works, but a local server is preferred for consistent
relative-link and browser-security behaviour.

The website intentionally links ROM downloads to GitHub Releases. ROM files
are release artifacts and must not be committed into this directory.

No production deployment or GitHub Pages configuration is included yet.
