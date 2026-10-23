#!/usr/bin/env bash
# Tests d'acceptation ha-mqtt-ui : rejoue les criteres de
# docs/specs/ha-mqtt-ui/features.json qui ne sont pas des tests natifs
# (greps, fichiers, scripts). Les commandes pio (build, buildfs, test natif)
# et `gh pr checks` sont lancees a part (contrat). Usage : depuis la racine.
cd "$(dirname "$0")/.." || exit 2
fail=0
chk() { # chk <id> <commande>
  if bash -c "$2" >/dev/null 2>&1; then echo "ok   $1"; else echo "FAIL $1 : $2"; fail=$((fail + 1)); fi
}

# F1
chk F1.a "! grep -nE 'asyncHTTPrequest|SimpleFTPServer|NTPClient' platformio.ini"
chk F1.b "grep -q 'AsyncPing' platformio.ini && grep -q 'TickerScheduler' platformio.ini"
chk F1.c "! grep -nE 'energyIndexHistoric|relayPlus|relaisPlus|relaisDistant|remoteRelay|boostTimer|timeScheduler|MAXPV_FTP_SERVER|configTzTime|logMqtt|espTransmitInfos|INDEX_ROUTED_J|/api/|DLconfig|WARNING LOOP TIME' MaxPV3/MaxPV3.ino MaxPV3/maxpv_defines.h MaxPV3/maxpv_options.h"
# F2
chk F2.a "grep -q '\"watchdogmqtt_active\"' MaxPV3/MaxPV3.ino && grep -q '\"temp_limit_active\"' MaxPV3/MaxPV3.ino && grep -q '\"dns1\"' MaxPV3/MaxPV3.ino"
chk F2.b "! grep -nE '\"(http_port|mqtt_active|boost_timer_[a-z]+|remoterelay_[a-z]+|RelayPlus_[a-z]+)\"' MaxPV3/MaxPV3.ino"
# F3
chk F3.a "grep -q 'MQTT_WATCHDOG_TIMEOUT' MaxPV3/maxpv_defines.h && grep -q 'MQTT_WATCHDOG_TIMEOUT' MaxPV3/MaxPV3.ino"
chk F3.b "grep -qE '^#define MQTT_WATCHDOG_TIMEOUT +600000' MaxPV3/maxpv_defines.h"
chk F3.c "grep -q 'watchDogWifi' MaxPV3/MaxPV3.ino && grep -q 'gatewayIP' MaxPV3/MaxPV3.ino"
# F4
chk F4.a "! grep -nE 'String\\(payload\\)' MaxPV3/MaxPV3.ino"
chk F4.b "! grep -nE '\"maxpv/(vrms|irms|pact|SYS)' MaxPV3/maxpv_defines.h MaxPV3/maxpv_mqtt.h MaxPV3/MaxPV3.ino"
chk F4.c "grep -q 'buildStateJson' MaxPV3/MaxPV3.ino && grep -q 'buildParamsJson' MaxPV3/MaxPV3.ino"
# F5
chk F5.a "test -f MaxPV3/maxpv_mqtt.h"
# F6
chk F6.a "grep -q 'handleCommand' MaxPV3/maxpv_mqtt.h && test \"\$(grep -c 'handleCommand' MaxPV3/MaxPV3.ino)\" -ge 2"
# F7
chk F7.a "for r in '\"/status\"' '\"/params\"' '\"/config\"' '\"/cmd\"' '\"/backup\"'; do grep -q \"\$r\" MaxPV3/MaxPV3.ino || exit 1; done"
chk F7.b "grep -q 'checkMutationHeader' MaxPV3/MaxPV3.ino && grep -q 'ElegantOTA.begin' MaxPV3/MaxPV3.ino && grep -q '405' MaxPV3/MaxPV3.ino && grep -q '404' MaxPV3/MaxPV3.ino"
chk F7.c "! grep -n 'redirect' MaxPV3/MaxPV3.ino"
chk F7.d "grep -q 'image/x-icon' MaxPV3/MaxPV3.ino"
# F8
chk F8.a "for f in index.html app.js pico.min.css maxpv.css favicon.ico versionweb.txt; do test -f MaxPV3/data/\$f || exit 1; done"
chk F8.b "for f in admin configuration credits graph main maj wizard; do test ! -e MaxPV3/data/\$f.html || exit 1; done"
chk F8.c "test \"\$(grep -cE 'id=\"tab-(etat|routeur|reseau|etalonnage|systeme)\"' MaxPV3/data/index.html)\" -eq 5"
chk F8.d "test -f MaxPV3/data/index.html && ! grep -nE '(src|href)=\"(https?:)?//[^\"]*\\.(js|css)' MaxPV3/data/index.html"
chk F8.e "test -f MaxPV3/data/app.js && ! grep -nE 'api/|cdn\\.|innerHTML *= *[a-zA-Z_]*\\.whatsnew' MaxPV3/data/app.js MaxPV3/data/index.html"
chk F8.f "grep -q 'X-MaxPV' MaxPV3/data/app.js && grep -q 'confirm(' MaxPV3/data/app.js && grep -q 'Pico' MaxPV3/data/pico.min.css"
# F9
chk F9.a "grep -qE '^#define MAXPV_VERSION +\"4.00\"' MaxPV3/maxpv_defines.h"
chk F9.a2 "grep -qE '^#define MAXPV_VERSION_FULL +\"MaxPV! 4.00\"' MaxPV3/maxpv_defines.h"
chk F9.b "test \"\$(tr -d '[:space:]' < MaxPV3/data/versionweb.txt)\" = 4.00"
chk F9.c ".venv/bin/python -I scripts/check_versions.py && .venv/bin/python -I scripts/check_versions.py --tag v4.00"
chk F9.d "! .venv/bin/python -I scripts/check_versions.py --tag v3.61"
# F10
chk F10.a "actionlint"
chk F10.b "! grep -n 'FTP' .github/workflows/build.yml"
chk F10.c "grep -q 'buildfs' .github/workflows/build.yml && grep -q 'merge-bin' .github/workflows/build.yml"
chk F10.d "grep -q 'pyyaml==6.0.2' .github/workflows/build.yml && grep -q 'check_blueprints.py' .github/workflows/build.yml"
# F11
chk F11.a "test -f blueprints/automation/maxpv/boost_programme.yaml && test -f blueprints/automation/maxpv/relayplus.yaml && test -f blueprints/automation/maxpv/relais_miroir.yaml"
chk F11.b ".venv/bin/python -I scripts/check_blueprints.py"
# F12
chk F12.a "test ! -e 'Documentation API' && test ! -e scripts/test_elegantota_v4.py && test ! -e images/mainpage.png"
chk F12.b "! grep -nE '^## (API|Relais distant|Accès au système de fichiers par FTP|Accès TELNET)' README.md && ! grep -n 'images/mainpage' README.md"
chk F12.c "grep -q 'MaxPV_full.bin' README.md && grep -q 'mosquitto_pub' README.md && grep -q 'blueprints/automation/maxpv' README.md && grep -qi 'Pico' README.md && grep -qE 'Jetblack31|Bernard Legrand' README.md && grep -q 'V 4.00' README.md"
chk F12.d "! grep -nE 'API_MaxPV|/api/' CLAUDE.md && grep -q 'handleCommand' CLAUDE.md && grep -q 'pio run -t buildfs -e d1_mini && ' CLAUDE.md"
chk F12.e "grep -q 'ha-mqtt-ui' docs/ROADMAP.md"
# F13 (hors pio et gh)
chk F13.a "git diff --quiet origin/main -- EcoPV3/EcoPV3.ino"

echo "echecs : $fail"
[ "$fail" -eq 0 ]
