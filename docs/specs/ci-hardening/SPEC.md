# SPEC : durcissement de la CI (ci-hardening)

## Contexte
La CI `.github/workflows/build.yml` est cassée : `actions/upload-artifact@v3` et `download-artifact@v3` sont refusées par GitHub depuis le 30/01/2025 (ROADMAP P0-1). Le workflow ne compile que le firmware ESP, avec des actions non épinglées, sans permissions explicites, sans versions d'outils figées, uniquement sur `push`. La release dépend d'une action tierce (`svenstaro/upload-release-action@v2`). Les binaires sont commités dans `Binaires MaxPV/`. Source : `docs/ROADMAP.md`, items CI-1 à CI-9.

Le fork gaetanars/MaxPV est autonome (pas de compatibilité amont imposée). Actions est activé sur le fork ; aucun tag n'existe.

## Objectif
Une CI qui fonctionne, reproductible, à privilèges minimaux, qui compile les trois firmwares (ESP, Nano, amorce), vérifie la cohérence des versions à la release et publie des assets de release à noms fixes utilisés par le README.

## Hors périmètre
- Corrections de code firmware ou web (P0-2 et suivants), y compris si `pio check` remonte des défauts.
- Modification des sketches (`*.ino`, `*.h`) et de `Librairies IDE/`.
- Le reste de P0-5 (URL `latest` dans `main.html`, liens amont, contenu de `latest`) : seuls les liens d'installation des binaires du README changent ici.
- Création d'un tag ou d'une release (fait par l'utilisateur après merge).
- Réglages GitHub du dépôt (branch protection, `sha_pinning_required`, etc.).
- Mise à jour de `docs/ROADMAP.md`.

## Exigences
1. **CI-1 Actions** : toutes les actions à leur dernière version majeure, épinglées par SHA de commit complet avec commentaire de version (`@<sha40> # vX.Y.Z`), via le skill `github-actions-pinning`. `upload-artifact`/`download-artifact` en v4 ou plus. `svenstaro/upload-release-action` remplacée par la CLI `gh`.
2. **CI-2 Permissions** : `permissions: contents: read` au niveau du workflow ; `contents: write` uniquement sur le job `release`.
3. **CI-3 Reproductibilité** :
   - `pip install platformio==<X.Y.Z>` (dernière version publiée au moment de la livraison) ; même version dans `.venv/` local.
   - `platformio.ini` : `platform = espressif8266@<X.Y.Z>` exact ; chaque `lib_deps` en version exacte `@X.Y.Z` = la dernière version dans la plage `^` actuelle. Aucun `^` ni `~` restant.
   - Python `3.13` dans le workflow.
   - Clé de cache incluant `hashFiles('platformio.ini', 'ci/*.ini')` (chemins `~/.cache/pip` et `~/.platformio`).
4. **CI-4 Déclencheurs** : `push` sur `main` et sur tags `v*` ; `pull_request` ; `workflow_dispatch`. `concurrency` : groupe `${{ github.workflow }}-${{ github.ref }}`, `cancel-in-progress` vrai sauf pour les refs de tag.
5. **CI-5 Compilations** (outil unique : PlatformIO), dans un seul job `build` séquentiel :
   - ESP : `pio run -e d1_mini` + `pio run -t buildfs -e d1_mini`, upload des `.bin` en artifact `maxpv-firmware` ;
   - ESP variante FTP : `PLATFORMIO_BUILD_FLAGS=-DMAXPV_FTP_SERVER`, dans un répertoire de build séparé (`PLATFORMIO_BUILD_DIR`), sans artifact ;
   - EcoPV : `pio ci EcoPV3/EcoPV3.ino --project-conf ci/ecopv.ini -e nano` et `-e nano_oled` ; `ci/ecopv.ini` définit `[env:nano]` (`platform = atmelavr@<exact>`, `board = nanoatmega328`, `framework = arduino`, `lib_deps` OneWire et DallasTemperature en versions exactes) et `[env:nano_oled]` (idem + `build_flags = -DOLED_128X64` + SSD1306Ascii en version exacte) ;
   - amorceESP : `pio ci amorceESP/amorceESP.ino --project-conf ci/amorce.ini -e amorce` ; `ci/amorce.ini` : `espressif8266` à la même version que `platformio.ini`, `board = d1_mini`, ElegantOTA en version exacte (la plus récente qui compile le sketch non modifié).
   - Les versions de libs sont la dernière publiée au moment de la livraison qui compile le source non modifié.
6. **CI-6 Cohérence des versions** : `scripts/check_versions.py` (Python 3 stdlib uniquement).
   - Lit `MAXPV_VERSION` et `MAXPV_VERSION_FULL` (`MaxPV3/maxpv_defines.h`), `MaxPV3/data/versionweb.txt`, `#define VERSION` (`EcoPV3/EcoPV3.ino`), et `latest` (JSON : `maxpvver`, `webver`, `ecopvver`).
   - Vérifie : `latest.maxpvver == MAXPV_VERSION`, `MAXPV_VERSION_FULL == "MaxPV! " + MAXPV_VERSION`, `latest.webver == versionweb.txt` (strip), `latest.ecopvver == VERSION`.
   - Option `--tag TAG` : vérifie en plus `TAG == "v" + MAXPV_VERSION`.
   - Option `--root DIR` (défaut : racine du dépôt déduite de l'emplacement du script).
   - Exit 0 si tout est cohérent ; sinon exit 1 et une ligne par incohérence sur stderr. Valeur introuvable = incohérence.
   - Exécuté dans le job `release` avec `--tag "$GITHUB_REF_NAME"`, avant toute publication.
7. **CI-7 Dependabot** : `.github/dependabot.yml`, `version: 2`, écosystème `github-actions`, répertoire `/`, intervalle `monthly`.
8. **CI-8 Release et binaires** :
   - Job `release` : uniquement si `startsWith(github.ref, 'refs/tags/v')`, `needs` = `lint` et `build`. Étapes : checkout, `check_versions.py --tag`, download artifact v4, copie de `MaxPV<ver>_firmware.bin` → `MaxPV3_firmware.bin` et `MaxPV<ver>_filesystem.bin` → `MaxPV3_filesystem.bin`, création de la release publiée si absente (`gh release create "$TAG" --verify-tag --generate-notes`), puis `gh release upload "$TAG" <4 fichiers> --clobber`. `GH_TOKEN: ${{ github.token }}`.
   - Suppression de `Binaires MaxPV/` (les deux `.bin`).
   - `README.md` : les instructions d'installation pointent vers `https://github.com/gaetanars/MaxPV/releases/latest/download/MaxPV3_filesystem.bin` et `.../MaxPV3_firmware.bin` ; plus aucune mention de `Binaires MaxPV`.
   - `CLAUDE.md` : section « Versions » mise à jour (tag `v<MAXPV_VERSION>`, assets fixes attachés par la CI, `scripts/check_versions.py`) ; plus aucune mention de `Binaires MaxPV` ; section « Build » : description de la CI à jour.
9. **CI-9 Analyse statique** : étape `pio check -e d1_mini` dans le job `build`, `continue-on-error: true` (non bloquante).
10. **Lint** : job `lint` exécutant actionlint à version exacte (binaire officiel téléchargé à une version figée), bloquant.

## Exigences de sécurité
- Aucun déclencheur `pull_request_target`.
- Aucune interpolation `${{ ... }}` de données contrôlables par un tiers (refs, titres, noms de branche) dans un bloc `run:` : passer par `env:`.
- `actions/checkout` avec `persist-credentials: false` dans les jobs qui ne poussent rien via git.
- Token en écriture limité au job `release`, exécuté seulement sur tag `v*`.
- Toute action tierce ou officielle épinglée par SHA complet.
- Téléchargement d'actionlint : version figée, et vérification du checksum publié si le script officiel ne le fait pas.

## Décisions prises
- Périmètre : CI-1 à CI-9 (P0-1 inclus).
- Outil unique de compilation : PlatformIO (`pio ci` pour EcoPV et amorceESP).
- Tags au format `v<MAXPV_VERSION>` (ex. `v3.62`).
- Versions épinglées = dernière dans la plage `^` actuelle (lib_deps ESP) ; dernière publiée qui compile le source non modifié (libs Nano/amorce, outils).
- Release publiée automatiquement (pas de brouillon).
- `Binaires MaxPV/` retiré dans cette livraison ; l'utilisateur pousse un tag juste après le merge pour combler l'absence de release.
- `pio check` non bloquant.
- actionlint en CI et en local (installé par l'utilisateur via Homebrew).
- Un seul fichier de workflow, jobs `lint`, `build`, `release`.
- `check_versions.py` en CI sur tag uniquement ; en local comme commande `test` du contrat.

## Risques
- Entre le merge et le premier tag, les liens README `releases/latest/download/...` renvoient 404.
- Le job `release` ne peut pas être testé de bout en bout avant le premier tag (pas de push de tag autorisé) : validé seulement par actionlint et relecture ; premier tag = test réel.
- La variante `nano_oled` ou `FTP` peut ne pas compiler sur le source actuel (taille flash, code). Dans ce cas, l'étape concernée passe en `continue-on-error: true`, avec l'erreur consignée dans `progress.md` et la PR (pas de modification du source).
- Monter les libs à la dernière version de la plage `^` peut changer le comportement runtime par rapport aux binaires actuels : la compilation est vérifiée, pas le comportement sur matériel.
- Dependabot proposera des PR de mise à jour des SHA ; elles déclenchent la CI.

## Taille retenue
M : un seul module (CI/release), ~10 fichiers, surface de sécurité limitée aux permissions du token et à la chaîne d'approvisionnement des actions.
