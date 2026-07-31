#!/usr/bin/env python3
"""Fail the build if active GBA code regains an external console dependency."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


ACTIVE_SUFFIXES = {
    ".bat", ".c", ".h", ".inc", ".mk", ".ps1", ".py", ".s", ".sh",
}
FORBIDDEN_FRAGMENTS = (
    "vendor/" + "builders",
    "builders/" + "snes",
    "builders/" + "nes",
    "tyrian_" + "snes",
    "tyrian_" + "nes",
    "load_" + "snes",
    "load_" + "nes",
    "super" + "nintendo",
    "snes" + "mod",
    "c:/ai_project/" + "aprtyriannes",
    "c:\\ai_project\\" + "aprtyriannes",
)


def active_files(root: Path) -> list[Path]:
    files = [
        root / "Makefile",
        root / "Configure.h",
        root / "main.c",
        root / "build.ps1",
    ]
    for directory in (root / "src", root / "tools"):
        files.extend(
            path
            for path in directory.rglob("*")
            if path.is_file()
            and path.suffix.lower() in ACTIVE_SUFFIXES
            and "portable-msys2" not in path.parts
            and "__pycache__" not in path.parts
            and path.name != Path(__file__).name
        )
    return sorted(set(path.resolve() for path in files if path.is_file()))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    root = args.project_root.resolve()
    output = args.output.resolve()
    if root not in output.parents:
        raise ValueError(f"audit output escaped project: {output}")

    violations: list[dict[str, object]] = []
    files = active_files(root)
    for path in files:
        normalized = path.read_text(
            encoding="utf-8", errors="replace"
        ).replace("\\", "/").lower()
        for fragment in FORBIDDEN_FRAGMENTS:
            needle = fragment.replace("\\", "/").lower()
            if needle in normalized:
                violations.append({
                    "file": path.relative_to(root).as_posix(),
                    "fragment": fragment,
                })

    forbidden_directories = [
        root / "vendor" / "builders",
    ]
    for path in forbidden_directories:
        if path.exists():
            violations.append({
                "file": path.relative_to(root).as_posix(),
                "fragment": "forbidden build directory exists",
            })

    reference_path = root / "vendor" / "audio" / "Music" / "gba-opl-reference.json"
    reference = json.loads(reference_path.read_text(encoding="utf-8"))
    if (
        reference.get("schema") != "tyrian-gba-opl-reference-v1"
        or reference.get("trackCount") != 41
        or len(reference.get("tracks", [])) != 41
        or any("profiles" in track for track in reference.get("tracks", []))
    ):
        violations.append({
            "file": reference_path.relative_to(root).as_posix(),
            "fragment": "reference is not the 41-track GBA-only OPL catalog",
        })

    music_manifest_path = root / "vendor" / "audio" / "Music" / "manifest.json"
    music_manifest = json.loads(music_manifest_path.read_text(encoding="utf-8"))
    source_description = str(music_manifest.get("source", ""))
    if (
        ":\\" in source_description
        or ":/" in source_description
        or "aprtyriannes" in source_description.lower()
    ):
        violations.append({
            "file": music_manifest_path.relative_to(root).as_posix(),
            "fragment": "music provenance contains a fixed external path",
        })

    report = {
        "schema": "tyrian-gba-project-independence-v1",
        "projectRoot": ".",
        "activeFilesScanned": len(files),
        "forbiddenDependencies": len(violations),
        "gbaOplReferenceTracks": int(reference.get("trackCount", 0)),
        "musicSource": source_description,
        "status": "pass" if not violations else "fail",
        "violations": violations,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    if violations:
        details = "; ".join(
            f"{item['file']}: {item['fragment']}" for item in violations
        )
        raise SystemExit(f"GBA project independence audit failed: {details}")
    print(
        "GBA project independence audit passed: "
        f"{len(files)} active files, 41 OPL reference tracks"
    )


if __name__ == "__main__":
    main()
