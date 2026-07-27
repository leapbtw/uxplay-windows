#!/usr/bin/env python3
"""Resolve stable GStreamer feature names to the current plugin DLLs.

Copy each owning plugin into the x64 bundle and record the resolved mapping.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from pathlib import Path

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst  # noqa: E402


def read_features(path: Path) -> list[str]:
    features: list[str] = []
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line and not line.startswith("#"):
            features.append(line)
    return features


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--features", type=Path, required=True)
    parser.add_argument("--plugin-dir", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()

    plugin_dir = args.plugin_dir.resolve()
    destination = args.destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)

    Gst.init(None)
    registry = Gst.Registry.get()
    mappings: list[dict[str, str]] = []
    missing: list[str] = []

    for feature_name in read_features(args.features):
        feature = registry.find_feature(feature_name, Gst.ElementFactory)
        if feature is None:
            missing.append(feature_name)
            continue

        plugin = feature.get_plugin()
        filename = plugin.get_filename() if plugin is not None else None
        if not filename:
            missing.append(feature_name)
            continue

        source = Path(filename).resolve()
        try:
            source.relative_to(plugin_dir)
        except ValueError:
            print(
                f"ERROR: feature {feature_name!r} resolved outside "
                f"{plugin_dir}: {source}",
                file=sys.stderr,
            )
            return 2

        target = destination / source.name
        if not target.exists():
            shutil.copy2(source, target)

        mappings.append(
            {
                "feature": feature_name,
                "plugin": plugin.get_name(),
                "file": source.name,
            }
        )

    if missing:
        print(
            "ERROR: missing GStreamer features: " + ", ".join(missing),
            file=sys.stderr,
        )
        return 3

    args.manifest.write_text(
        json.dumps(mappings, indent=2, sort_keys=True) + os.linesep,
        encoding="utf-8",
    )

    unique_plugins = sorted({entry["file"] for entry in mappings})
    print(
        f"Resolved {len(mappings)} features to "
        f"{len(unique_plugins)} GStreamer plugins."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
