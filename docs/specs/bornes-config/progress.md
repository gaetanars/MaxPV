# Journal bornes-config

date | step | result
2026-10-22 20:47:43 | start | branche agent/bornes-config, taille S
2026-10-22 21:01:49 | tests | critères F1/F2 écrits en grep : 16 KO avant code, 0 KO après
2026-10-22 21:01:49 | implémentation | helper configBounded + 12 champs bornés dans MaxPV3/MaxPV3.ino
2026-10-22 21:01:49 | contrat | check_versions OK, actionlint OK, buildfs OK, EcoPV nano OK (Docker amd64)
2026-10-22 21:01:49 | contrat | pio run -e d1_mini local/Docker : segfault cc1plus sous émulation sur fichiers de libs différents (AsyncMqttClient, ESP8266mDNS), hors code modifié ; validation laissée à la CI, F3 reste passes=false
2026-10-22 21:01:49 | hypothèse | pas de montée de version (hors périmètre)
