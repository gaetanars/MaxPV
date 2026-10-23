#!/usr/bin/env python3
"""Vérifie les blueprints Home Assistant de blueprints/**/*.yaml."""
import sys
from pathlib import Path

import yaml


class Input:
    """Valeur d'un tag !input (nom de l'input référencé)."""

    def __init__(self, name):
        self.name = name


class Loader(yaml.SafeLoader):
    pass


def _input(loader, node):
    return Input(loader.construct_scalar(node))


Loader.add_constructor("!input", _input)


def walk(node):
    """Itère sur tous les noeuds d'un document chargé."""
    yield node
    if isinstance(node, dict):
        for k, v in node.items():
            yield from walk(k)
            yield from walk(v)
    elif isinstance(node, list):
        for v in node:
            yield from walk(v)


def declared_inputs(inputs):
    """Noms des inputs, sections (`input:` imbriqués) aplaties."""
    names = set()
    for name, value in inputs.items():
        if isinstance(value, dict) and isinstance(value.get("input"), dict):
            names |= declared_inputs(value["input"])
        else:
            names.add(name)
    return names


def check(path):
    errors = []
    try:
        doc = yaml.load(path.read_text(encoding="utf-8"), Loader=Loader)
    except (OSError, yaml.YAMLError) as e:
        return [f"YAML invalide : {e}"]
    if not isinstance(doc, dict) or not isinstance(doc.get("blueprint"), dict):
        return ["clé blueprint absente"]
    bp = doc["blueprint"]
    if not bp.get("name"):
        errors.append("blueprint.name absent")
    if bp.get("domain") != "automation":
        errors.append("blueprint.domain doit valoir automation")
    inputs = bp.get("input")
    if not isinstance(inputs, dict) or not inputs:
        errors.append("blueprint.input absent ou vide")
        return errors
    declared = declared_inputs(inputs)
    used = {n.name for n in walk(doc) if isinstance(n, Input)}
    for name in sorted(declared - used):
        errors.append(f"input déclaré mais jamais utilisé : {name}")
    for name in sorted(used - declared):
        errors.append(f"!input vers un input non déclaré : {name}")
    return errors


def main():
    root = Path(__file__).resolve().parent.parent
    files = sorted((root / "blueprints").glob("**/*.yaml"))
    if not files:
        print("FAIL aucun blueprint trouvé")
        return 1
    failed = 0
    for f in files:
        errs = check(f)
        rel = f.relative_to(root)
        if errs:
            failed += 1
            for e in errs:
                print(f"FAIL {rel} : {e}")
        else:
            print(f"ok   {rel}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
