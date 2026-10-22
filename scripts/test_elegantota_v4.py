#!/usr/bin/env python3
"""Tests d'acceptation elegantota-v4 : exécute les critères F1, F2, F4, F5 de
docs/specs/elegantota-v4/features.json (F3 et F6 dépendent de la PR/CI)."""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FEATURES = ROOT / "docs" / "specs" / "elegantota-v4" / "features.json"
# F5 : check_versions et actionlint relèvent du contrat, pas de ce test.
SKIP = ("check_versions.py", "actionlint")


def main():
    failed = 0
    for feat in json.loads(FEATURES.read_text(encoding="utf-8")):
        if feat["id"] in ("F3", "F6"):
            continue
        for crit in feat["criteres_acceptation"]:
            if any(s in crit for s in SKIP):
                continue
            r = subprocess.run(["bash", "-c", crit], cwd=ROOT,
                               capture_output=True, text=True)
            ok = r.returncode == 0
            print(f"{'PASS' if ok else 'FAIL'} {feat['id']}: {crit}")
            failed += not ok
    print(f"{failed} critère(s) en échec")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
