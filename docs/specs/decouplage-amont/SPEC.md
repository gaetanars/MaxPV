# SPEC : découplage de l'amont Jetblack31 (decouplage-amont)

## Contexte
Le fork gaetanars/MaxPV est autonome (pas de PR amont, pas de compatibilité imposée avec Jetblack31/MaxPV). Pourtant, l'UI embarquée dépend encore de l'amont (ROADMAP P0-5) :
- `MaxPV3/data/main.html:107` : `urlVersion = "https://raw.githubusercontent.com/Jetblack31/MaxPV/main/latest"`. Dès que les versions divergent, l'UI annonce à tort des mises à jour vers les versions amont.
- `main.html:172,180` : le `whatsnew` du fichier `latest` distant est concaténé puis injecté via `innerHTML` dans `#livemsg`, donc du contenu distant s'exécute dans l'UI locale (ROADMAP P3-4). Les numéros de version distants (`result4[...]`) sont injectés de la même façon.
- `main.html:91` : le lien « Aide disponible ici » pointe vers Jetblack31/MaxPV.
- `main.html:86-90` et `credits.html:62-64` : bouton « Je soutiens MaxPV! » vers `https://paypal.me/EcoPV` (auteur amont) ; `.github/FUNDING.yml` : même lien.
- `credits.html:61` : « Plus d'informations sur » pointe vers Jetblack31/MaxPV.
- `latest:5` : `whatsnew` en HTML avec un lien vers Jetblack31/MaxPV.
- `CLAUDE.md:38` : indique que `main.html` lit `latest` sur le dépôt Jetblack31/MaxPV. `CLAUDE.md:46` présente Jetblack31/MaxPV comme « Dépôt amont ».
Les liens d'installation du README pointent déjà vers le fork (livré par ci-hardening).

## Objectif
L'UI vérifie les mises à jour sur le fork, affiche le contenu distant en texte brut uniquement, et ne renvoie plus vers l'amont sauf pour l'attribution (copyright/licence) et la documentation EcoPV.

## Hors périmètre
- Sources des sketches (`*.ino`, `*.h`), y compris les en-têtes de copyright qui citent Jetblack31.
- `MaxPV3/data/configuration.html:64` (aide EcoPV amont) et `README.md:8` (lien EcoPV) : contenu documentaire, conservés.
- `credits.html:94-95` (bloc « Copyright et licence ») : attribution LGPL, conservés.
- Montée des numéros de version (`versionweb.txt`, `MAXPV_VERSION`, `VERSION` EcoPV, champs de version de `latest`), entrée README « Versions », tag et release : faits par l'utilisateur.
- Les autres usages d'`innerHTML` alimentés par l'API locale (`ecopvver`, `maxpvver`, `webver`, états).
- `docs/ROADMAP.md` (non suivi par git).
- `.github/workflows/`.

## Exigences
1. **URL de version** : dans `MaxPV3/data/main.html`, `urlVersion` vaut exactement `"https://raw.githubusercontent.com/gaetanars/MaxPV/main/latest"`.
2. **Affichage en texte brut (P3-4)** : dans `fillVersion()`, `#livemsg` n'est plus jamais écrit via `innerHTML` mais via `textContent`. `messageVersion` est construit avec des sauts de ligne `"\n"` à la place des `<br>`, en gardant le même texte et la même logique (une ligne par composant à mettre à jour, puis `whatsnew`, ou « Le système est à jour ! »). Le message initial « Recherche de mise à jour... » passe aussi par `textContent`. L'élément `<span id="livemsg">` reçoit `style="white-space: pre-line"` et son contenu initial `<br>` est vidé. Les classes `text-warning`, `text-danger` et `text-success` restent inchangées.
3. **Fichier `latest`** : JSON valide. `ecopvver`, `maxpvver` et `webver` sont inchangés (3.60 / 3.61 / 3.63). `whatsnew` devient du texte brut, sans `<`, `>` ni référence à Jetblack31 : `"Nouveautés : Correction des sources script pour affichage des graphiques. Voir les informations sur GitHub : https://github.com/gaetanars/MaxPV"`. Le format d'indentation existant est conservé.
4. **Lien d'aide** : `main.html:91`, le lien « Aide disponible ici » pointe vers `https://github.com/gaetanars/MaxPV`.
5. **PayPal retiré** : dans `main.html`, le `<div>` qui contient le bouton « Je soutiens MaxPV! » est supprimé. Dans `credits.html`, le `<a href="https://paypal.me/EcoPV" ...>` et son bouton sont supprimés. `.github/FUNDING.yml` est supprimé. Aucune occurrence de `paypal` (insensible à la casse) ne subsiste dans `MaxPV3/data/` ni `.github/`.
6. **Crédits** : `credits.html:61`, le lien « Plus d'informations sur » pointe vers `https://github.com/gaetanars/MaxPV`, avec ce même texte affiché. Les lignes 94-95 (Jetblack31/EcoPV et Jetblack31/MaxPV) sont conservées telles quelles.
7. **CLAUDE.md** : la ligne 38 indique que `main.html` lit `latest` via raw.githubusercontent.com sur le dépôt `gaetanars/MaxPV` (branche `main`) et que `whatsnew` est affiché en texte brut. La ligne 46 devient : `- Origine : fork de Jetblack31/MaxPV (licence LGPL 2.1+ en en-têtes, attribution conservée dans credits.html) ; ce fork est autonome, l'UI ne référence plus l'amont hors crédits et aide EcoPV.`
8. **Cohérence** : `.venv/bin/python -I scripts/check_versions.py` reste à 0.

## Exigences de sécurité
- Aucune donnée issue de la requête distante (`latest` : versions et `whatsnew`) n'est injectée via `innerHTML`, `outerHTML`, `insertAdjacentHTML` ni `document.write`.
- Aucune nouvelle ressource externe (script, CSS, requête) n'est ajoutée.

## Décisions prises
- `urlVersion` vers raw `gaetanars/MaxPV/main/latest` (pas de suppression de la vérification, pas d'API releases).
- P3-4 inclus : rendu en `textContent`, `whatsnew` en texte brut (le lien n'est plus cliquable, ce qui est accepté).
- Lien d'aide et « Plus d'informations » vers le fork ; PayPal amont retiré (UI + FUNDING.yml).
- Pas de montée de version web dans ce lot.

## Risques
- Le build local est impossible sur macOS arm64 (toolchain xtensa x86_64). La compilation du filesystem (`buildfs`) est validée par la CI de la PR, ou via Docker `python:3.13` comme dans les livraisons précédentes.
- Avant la prochaine release, l'UI déjà flashée sur les appareils (web 3.63) lit toujours l'amont : le changement ne prend effet qu'après l'OTA du filesystem.
- Tant que `latest` sur `main` reflète la dernière version, les appareils à jour affichent « à jour ». Au moment d'une release, `latest` doit être mergé sur `main`, ce que garantit déjà `check_versions.py` au tag.

## Taille retenue
M : 6 fichiers (main.html, credits.html, latest, FUNDING.yml, CLAUDE.md, plus la spec), un seul module (frontend et métadonnées), surface de sécurité limitée (rendu du contenu distant).
