#!/usr/bin/env python3
"""Validate the dependency-free TyrianGbaPoc static website.

Checks local links/fragments, duplicate IDs, Traditional-Chinese translation
coverage, 240x160 gallery PNG dimensions, and removal of retired articles.
Only Python's standard library is required.
"""

from __future__ import annotations

import argparse
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import struct
import sys
from urllib.parse import unquote, urlsplit


CHINESE_RE = re.compile(r"[\u3400-\u9fff]")
CATALOG_KEY_RE = re.compile(r'^\s*"((?:[^"\\]|\\.)*)"\s*:')
SKIP_TEXT_TAGS = {"script", "style", "pre"}
TRANSLATED_ATTRIBUTES = {"aria-label", "alt"}
RETIRED_PATHS = {
    "research/lava-water-wave-pressure.html",
    "assets/images/research/lava-wave-v71-before.png",
    "assets/images/research/lava-wave-v72-after.png",
}


def normalize(value: str) -> str:
    return " ".join(value.split())


def read_catalog(path: Path) -> tuple[set[str], set[str]]:
    keys: set[str] = set()
    duplicates: set[str] = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        match = CATALOG_KEY_RE.match(line)
        if match:
            key = json.loads(f'"{match.group(1)}"')
            if key in keys:
                duplicates.add(key)
            keys.add(key)
    return keys, duplicates


class PageParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.skip_depth = 0
        self.links: list[tuple[str, str]] = []
        self.ids: list[str] = []
        self.translatable: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag in SKIP_TEXT_TAGS:
            self.skip_depth += 1
        values = dict(attrs)
        if values.get("id"):
            self.ids.append(values["id"] or "")
        for name in ("href", "src"):
            if values.get(name):
                self.links.append((name, values[name] or ""))
        for name in TRANSLATED_ATTRIBUTES:
            value = values.get(name)
            if value and CHINESE_RE.search(value):
                self.translatable.append(normalize(value))
        if tag == "meta" and values.get("name") == "description":
            value = values.get("content")
            if value and CHINESE_RE.search(value):
                self.translatable.append(normalize(value))

    def handle_startendtag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self.handle_starttag(tag, attrs)
        if tag in SKIP_TEXT_TAGS:
            self.skip_depth -= 1

    def handle_endtag(self, tag: str) -> None:
        if tag in SKIP_TEXT_TAGS and self.skip_depth:
            self.skip_depth -= 1

    def handle_data(self, data: str) -> None:
        value = normalize(data)
        if not self.skip_depth and value and CHINESE_RE.search(value):
            self.translatable.append(value)


def parse_page(path: Path) -> PageParser:
    parser = PageParser()
    parser.feed(path.read_text(encoding="utf-8"))
    return parser


def png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError("not a valid PNG IHDR")
    return struct.unpack(">II", header[16:24])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--site",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "Website",
    )
    args = parser.parse_args()
    site = args.site.resolve()
    errors: list[str] = []
    html_paths = sorted(site.rglob("*.html"))
    catalog, duplicate_catalog_keys = read_catalog(site / "assets" / "js" / "i18n.js")
    pages = {path: parse_page(path) for path in html_paths}

    for key in sorted(duplicate_catalog_keys):
        errors.append(f"duplicate English catalog key: {key}")

    for path, page in pages.items():
        rel = path.relative_to(site).as_posix()
        duplicates = sorted({item for item in page.ids if page.ids.count(item) > 1})
        for item in duplicates:
            errors.append(f"{rel}: duplicate id #{item}")
        for value in page.translatable:
            if value not in catalog:
                errors.append(f"{rel}: missing English catalog entry: {value}")

        for kind, raw in page.links:
            split = urlsplit(raw)
            if split.scheme or split.netloc or raw.startswith(("mailto:", "data:")):
                continue
            local_path = unquote(split.path)
            target = path if not local_path else (path.parent / local_path).resolve()
            try:
                target.relative_to(site)
            except ValueError:
                errors.append(f"{rel}: {kind} escapes Website/: {raw}")
                continue
            if target.is_dir():
                target = target / "index.html"
            if not target.exists():
                errors.append(f"{rel}: broken local {kind}: {raw}")
                continue
            if split.fragment and target.suffix.lower() == ".html":
                target_page = pages.get(target) or parse_page(target)
                if split.fragment not in target_page.ids:
                    errors.append(f"{rel}: missing fragment in {raw}")

    for retired in RETIRED_PATHS:
        if (site / retired).exists():
            errors.append(f"retired page/asset still exists: {retired}")
    for path in site.rglob("*"):
        if path.is_file() and "lava-water-wave-pressure" in path.read_text(
            encoding="utf-8", errors="ignore"
        ):
            errors.append(f"retired article reference remains: {path.relative_to(site)}")

    gallery = site / "assets" / "images" / "gallery"
    for image in sorted(gallery.glob("*.png")):
        try:
            dimensions = png_dimensions(image)
        except ValueError as exc:
            errors.append(f"{image.relative_to(site)}: {exc}")
            continue
        if dimensions != (240, 160):
            errors.append(
                f"{image.relative_to(site)}: expected 240x160, got {dimensions[0]}x{dimensions[1]}"
            )

    if errors:
        print("Website audit FAILED:")
        for error in errors:
            print(f"- {error}")
        return 1
    print(
        f"Website audit PASS: {len(html_paths)} HTML pages, "
        f"{len(catalog)} translations, "
        f"{len(list(gallery.glob('*.png')))} 240x160 gallery images."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
