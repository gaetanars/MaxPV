# Journal ci-hardening

date | step | result
2026-10-22 17:48:26 | env | .venv + pio 6.2.0 ; AsyncElegantOTA absente du registre PIO -> épinglée via git tag v2.2.9 (dernier 2.x, hypothèse); deps git transitives (ESPAsyncTCP etc.) non épinglables
2026-10-22 17:49:14 | env | toolchain xtensa x86_64 sans Rosetta -> build local impossible sur macOS arm64; tentative via docker python:3.13 ; platformio.ini: $PROJECT_DIR/ retiré des chemins (pio 6.2.0 doublait le chemin)
2026-10-22 17:52:40 | libs | AsyncElegantOTA v2.2.9 = stub #error deprecated -> v2.2.8 ; builds via docker python:3.13 : d1_mini+buildfs, nano (81%), nano_oled (93.6%), amorce(ElegantOTA 4.0.0) OK
2026-10-22 17:54:39 | hook | guard-bash a bloqué rm -rf $T ; tests F5 refaits dans le scratchpad sans rm -r
2026-10-22 17:56:03 | revue | reviewer: 0 bloquant ; corrigés: CLAUDE.md:11, vérif 4 fichiers dans release. pio check échoue (cppcheck, docker arm64) -> non bloquant. F4 validé via docker faute de Rosetta (contrat build local inexécutable). F10 en attente du run de la PR
2026-10-22 17:59:53 | F10 | run PR success (lint, build verts ; release skipped)
