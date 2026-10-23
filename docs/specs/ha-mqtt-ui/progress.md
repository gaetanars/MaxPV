# Journal ha-mqtt-ui

date | step | result
2026-10-23 09:01 | start | branche agent/ha-mqtt-ui créée depuis main, taille L
2026-10-23 09:01 | hypothèse | docs/ROADMAP.md était non suivi ; commité sur la branche car F12 doit le modifier et les worktrees du workflow ne le verraient pas
2026-10-23 09:07 | workflow livrer-large | arrêt partiel à l'étape Tests (2 agents : architect, tests) ; implémentation non commencée ; blockers signalés : signatures de maxpv_mqtt.h fixées par les tests, hors-bornes MQTT/cmd rejeté (hypothèse), source de pico.min.css (réseau), gh pr checks nécessitant la PR
2026-10-23 09:28 | décision utilisateur | hors bornes en commande = rejeté ; pico.min.css 2.1.1 téléchargé depuis jsdelivr (le navigateur appelle déjà internet pour latest) ; reprise de l'implémentation
2026-10-23 09:36 | reprise /livrer | branche existante, marqueur réactivé, relance livrer-large (hors bornes = rejeté ; pico.min.css déjà commité)
2026-10-23 10:09 | workflow livrer-large | ok, 8 agents, séquentiel, 0 boucle de correction, 0 conflit ; contrat rejoué à la main : versions, blueprints, actionlint, 3 builds Docker arm64, 24 tests natifs, acceptation 0 échec, EcoPV3.ino intact ; F13 en attente des runs CI de la PR
2026-10-23 10:09 | hypothèses | relayplus.yaml : en force, passage en stop une fois la durée min atteinte (comportement 3.x, non demandé par la spec) ; assistant UI : p_offset arrondi à l'entier, ordre indx0/resetecopv/v_calib de l'ancien assistant ; UI non testée en navigateur
2026-10-23 10:09 | revue sécurité | seul point : X-MaxPV sans authentification, statu quo P1-1
