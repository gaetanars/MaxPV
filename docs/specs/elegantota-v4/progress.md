# Journal elegantota-v4

date | step | result
2026-10-22 19:26:45 | start | branche agent/elegantota-v4, taille L
2026-10-22 19:30:48 | workflow livrer-large | partial (stage Implémentation) : T2, T3 faits ; T1 bloqué
2026-10-22 19:30:48 | T1 blocage | pio pkg list : 3 ESPAsyncTCP (esp32async 2.0.0 ; me-no-dev 2.0.0 via AsyncHTTPRequest_Generic ; khoih-prog/ESPAsyncTCP git non épinglé via AsyncMQTT_Generic 1.8.1). lib_ignore agit par nom : impossible d'écarter le fork khoih seul. Remplacer AsyncMQTT_Generic = décision d'architecture (exig. 3b), non prise.
2026-10-22 19:30:48 | contrat | test OK, lint OK, build local impossible (toolchain xtensa x86_64 vs arm64), F2 pio pkg list ESPAsyncTCP = FAIL
2026-10-22 19:30:48 | note | sous-agent T3 signalé "Self-Modification" par le harnais : il a édité CLAUDE.md, ce que la SPEC (exig. 6) demande ; diff relu, conforme.
2026-10-22 19:30:48 | stop | ÉCHEC PARTIEL, PR draft
2026-10-22 19:54:29 | déblocage F2 | AsyncMQTT_Generic (archivé, tire un fork git d'ESPAsyncTCP) -> heman/AsyncMqttClient-esphome 2.1.0 (MIT, même API, ESPHome, 2024) ; AsyncHTTPRequest_Generic (archivé, GPL-3.0, tire WiFiManager/AsyncTCP/STM32) -> boblemaire/asyncHTTPrequest 1.2.2 (LGPL-3.0, original dont Generic est le fork ; classe asyncHTTPrequest, états 0/4 car enum privé) ; lib_ignore ESPAsyncTCP-esphome (dépendance transitive, doublon d'esp32async/ESPAsyncTCP) ; STM32AsyncTCP n'est plus tiré
2026-10-22 19:56:23 | CI | clearQueue() absent du fork ESPHome ; supprimé (la file est vidée par _onDisconnect -> _clear())
2026-10-22 20:00:17 | CI | run 39827035122 vert (build + variante FTP + EcoPV + amorce, lint). Flash 514895 -> 475981 o, RAM 40368 -> 40620 o. Portail ElegantOTA décompressé : aucune ressource CDN ni télémétrie, seul lien https://elegantota.pro
