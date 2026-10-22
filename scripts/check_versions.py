#!/usr/bin/env python3
"""Vérifie la cohérence des numéros de version du dépôt (et du tag de release)."""
import argparse
import json
import re
import sys
from pathlib import Path


def read(path):
    try:
        return path.read_text(encoding="utf-8")
    except OSError:
        return None


def find(text, pattern):
    if text is None:
        return None
    m = re.search(pattern, text, re.MULTILINE)
    return m.group(1) if m else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", help="tag de release attendu (v<MAXPV_VERSION>)")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent,
                        help="racine du dépôt (défaut : déduite du script)")
    args = parser.parse_args()
    root = args.root

    defines = read(root / "MaxPV3" / "maxpv_defines.h")
    maxpv = find(defines, r'^#define\s+MAXPV_VERSION\s+"([^"]*)"')
    maxpv_full = find(defines, r'^#define\s+MAXPV_VERSION_FULL\s+"([^"]*)"')
    web = read(root / "MaxPV3" / "data" / "versionweb.txt")
    web = web.strip() if web is not None else None
    ecopv = find(read(root / "EcoPV3" / "EcoPV3.ino"), r'^#define\s+VERSION\s+"([^"]*)"')

    latest = {}
    try:
        latest = json.loads(read(root / "latest") or "")
    except ValueError:
        pass
    if not isinstance(latest, dict):
        latest = {}

    errors = []

    def expect(label, actual, expected, expected_label):
        if actual is None:
            errors.append(f"{label} introuvable")
        elif expected is None:
            errors.append(f"{expected_label} introuvable")
        elif actual != expected:
            errors.append(f"{label} = {actual!r} != {expected_label} = {expected!r}")

    expect("latest.maxpvver", latest.get("maxpvver"), maxpv, "MAXPV_VERSION")
    expect("MAXPV_VERSION_FULL", maxpv_full,
           "MaxPV! " + maxpv if maxpv is not None else None, '"MaxPV! " + MAXPV_VERSION')
    expect("latest.webver", latest.get("webver"), web, "versionweb.txt")
    expect("latest.ecopvver", latest.get("ecopvver"), ecopv, "EcoPV3.ino VERSION")
    if args.tag is not None:
        expect("tag", args.tag, "v" + maxpv if maxpv is not None else None, '"v" + MAXPV_VERSION')

    for e in errors:
        print(e, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
