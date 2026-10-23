'use strict';

// Paramètres EcoPV : clé, libellé, aide, min, max, pas (bornes de pvrParamConfig)
const PARAMS = [
  ['p_installpv', 'P_INSTALLPV', "Puissance de l'installation photovoltaïque (Wc)", 100, 30000, 1],
  ['p_resistance', 'P_RESISTANCE', 'Puissance de la résistance commandée (W)', 100, 10000, 1],
  ['cnt_calib', 'CNT_CALIB', "Poids des impulsions du compteur d'impulsions (Wh)", 0, 1000, 0.0001],
  ['v_calib', 'V_CALIB', 'Facteur de calibrage de la tension', 0, 2, 0.0001],
  ['p_calib', 'P_CALIB', 'Facteur de calibrage de la puissance', 0.0001, 1, 0.0001],
  ['phase_calib', 'PHASE_CALIB', 'Facteur de calibrage de la phase', -16, 48, 1],
  ['p_offset', 'P_OFFSET', 'Décalage de puissance active (W)', -100, 100, 1],
  ['p_margin', 'P_MARGIN', 'Consigne de régulation (W)', -2000, 2000, 1],
  ['gain_p', 'GAIN_P', 'Gain proportionnel de régulation', 0, 1000, 1],
  ['gain_i', 'GAIN_I', 'Gain intégral de régulation', 0, 1000, 1],
  ['e_reserve', 'E_RESERVE', 'Tolérance de régulation (J)', 0, 200, 1],
  ['p_div2_active', 'P_DIV2_ACTIVE', 'Excédent de production pour relais ON (W)', 0, 9999, 1],
  ['p_div2_idle', 'P_DIV2_IDLE', 'Importation minimale pour relais OFF (W)', 0, 9999, 1],
  ['t_div2_on', 'T_DIV2_ON', 'Durée minimale ON du relais (min)', 0, 240, 1],
  ['t_div2_off', 'T_DIV2_OFF', 'Durée minimale OFF du relais (min)', 0, 240, 1],
  ['t_div2_tc', 'T_DIV2_TC', 'Constante de lissage du relais (min)', 0, 60, 1]
];
const ESP_PARAMS = [
  ['boost_duration', 'Durée du BOOST (min)', 10, 600, 1],
  ['boost_ratio', 'Puissance du BOOST (%)', 0, 100, 1],
  ['max_temp', 'Température max ECS (°C)', 15, 90, 1]
];
const NET_KEYS = ['ip', 'gateway', 'subnet', 'dns1', 'dns2', 'watchdogwifi_active', 'watchdogmqtt_active',
  'mqtt_ip', 'mqtt_port', 'mqtt_user', 'mqtt_period'];
const LATEST_URL = 'https://raw.githubusercontent.com/gaetanars/MaxPV/main/latest';
const TABS = ['etat', 'routeur', 'reseau', 'etalonnage', 'systeme'];

const $ = (id) => document.getElementById(id);
let status = {};
let params = {};
let boostOn = false;
let cosMax = 0;
let timer = null;

// ---------------------------------------------------------------- réseau

function show(text, ok) {
  const m = $('msg');
  m.textContent = text;
  m.className = ok ? 'ok' : 'err';
  clearTimeout(show.t);
  if (text) show.t = setTimeout(() => { m.textContent = ''; m.className = ''; }, 5000);
}

// Seul point d'envoi : POST form-urlencoded avec l'en-tête X-MaxPV. Retourne true si 200.
async function post(url, params) {
  try {
    const r = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded', 'X-MaxPV': '1' },
      body: new URLSearchParams(params).toString()
    });
    if (r.ok) {
      show('Envoyé', true);
      return true;
    }
    show('Refusé (' + r.status + ') : ' + (await r.text()), false);
  } catch (e) {
    show('Erreur réseau', false);
  }
  return false;
}

async function getJson(url) {
  const r = await fetch(url, { cache: 'no-store' });
  if (!r.ok) throw new Error(String(r.status));
  return r.json();
}

function cmd(key, value) {
  return post('/cmd', { key: key, value: String(value) });
}

// Envoi séquentiel : chaque commande attend la réponse de la précédente
async function seq(list) {
  for (const [k, v] of list) {
    if (!(await cmd(k, v))) return false;
  }
  return true;
}

// ---------------------------------------------------------------- onglets

function showTab() {
  let t = location.hash.replace('#', '');
  if (TABS.indexOf(t) < 0) t = 'etat';
  TABS.forEach((n) => {
    $('tab-' + n).hidden = (n !== t);
  });
  document.querySelectorAll('nav[aria-label="onglets"] a').forEach((a) => {
    if (a.getAttribute('href') === '#' + t) a.setAttribute('aria-current', 'page');
    else a.removeAttribute('aria-current');
  });
  if (t === 'routeur') loadParams();
  if (t === 'reseau') loadConfig();
  if (t === 'etalonnage') loadParams();
  schedule();
}

// ---------------------------------------------------------------- état

function fmtUptime(s) {
  s = Number(s) || 0;
  const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
  return (d ? d + ' j ' : '') + h + ' h ' + m + ' min';
}

const MODES = { 0: 'stop', 1: 'force' };
function modeName(c) { return MODES[c] || 'auto'; }

function fill(s) {
  document.querySelectorAll('[data-k]').forEach((e) => {
    const k = e.getAttribute('data-k');
    let v = s[k];
    if (v === undefined) return;
    if (k === 'uptime') v = fmtUptime(v);
    e.textContent = String(v);
  });
  $('h-nano').textContent = 'Nano : ' + (s.nano === 'ON' ? 'connecté' : 'absent');
  $('h-mqtt').textContent = 'MQTT : ' + (s.mqtt === 'ON' ? 'connecté' : 'déconnecté');
  $('h-ver').textContent = 'version ' + (s.maxpv_version || '?');
  $('v-maxpv').textContent = s.maxpv_version || '-';
  $('v-ecopv').textContent = s.ecopv_version || '-';
  $('mqtt-id').textContent = s.mqtt_id || '-';
  $('mqtt-prefix').textContent = s.mqtt_id ? 'maxpv/' + s.mqtt_id + '/' : '-';
  const sel = (id, v) => { if (document.activeElement !== $(id)) $(id).value = modeName(v); };
  sel('sel-triac_mode', s.triac_mode);
  sel('sel-relay_mode', s.relay_mode);
  boostOn = (s.boost === 'ON');
  $('btn-boost').textContent = boostOn ? 'Arrêter le BOOST' : 'Lancer le BOOST';
  // étalonnage
  $('w-vrms').textContent = s.vrms === undefined ? '-----' : String(s.vrms);
  const c = Math.abs(Number(s.cosphi));
  if (!isNaN(c)) {
    if (c > cosMax) cosMax = c;
    $('w-cosphi').textContent = String(c);
    $('w-cosmax').textContent = String(cosMax);
    $('w-cosbar').value = Math.min(1.02, Math.max(0.98, c));
  }
}

async function refresh() {
  try {
    status = await getJson('/status');
    fill(status);
  } catch (e) {
    $('h-nano').textContent = 'Nano : ?';
    $('h-mqtt').textContent = 'MQTT : ?';
  }
}

function schedule() {
  clearTimeout(timer);
  const run = async () => {
    await refresh();
    timer = setTimeout(run, location.hash === '#etalonnage' ? 1000 : 5000);
  };
  run();
}

// ---------------------------------------------------------------- routeur

function buildField(container, key, label, help, min, max, step) {
  const wrap = document.createElement('div');
  wrap.className = 'row-param';
  const l = document.createElement('label');
  l.textContent = label;
  if (help) {
    const sm = document.createElement('small');
    sm.textContent = help;
    l.appendChild(sm);
  }
  const inp = document.createElement('input');
  inp.type = 'number';
  inp.id = 'p-' + key;
  inp.min = min;
  inp.max = max;
  inp.step = step;
  l.appendChild(inp);
  const b = document.createElement('button');
  b.type = 'button';
  b.textContent = 'Envoyer';
  b.addEventListener('click', async () => {
    if (await cmd(key, inp.value)) loadParams();
  });
  wrap.appendChild(l);
  wrap.appendChild(b);
  container.appendChild(wrap);
}

function buildParams() {
  const a = $('params-ecopv');
  PARAMS.forEach((p) => buildField(a, p[0], p[1], p[2], p[3], p[4], p[5]));
  const b = $('params-esp');
  ESP_PARAMS.forEach((p) => buildField(b, p[0], p[1], '', p[2], p[3], p[4]));
}

async function loadParams() {
  try {
    params = await getJson('/params');
  } catch (e) {
    return;
  }
  Object.keys(params).forEach((k) => {
    const e = $('p-' + k);
    if (e && document.activeElement !== e) e.value = params[k];
  });
  $('chk-temp_limit').checked = (Number(params.temp_limit) === 1);
  // étalonnage
  const w = { 'w-p_installpv': 'p_installpv', 'w-p_resistance': 'p_resistance', 'w-cnt_calib': 'cnt_calib', 'w-phase_calib': 'phase_calib' };
  Object.keys(w).forEach((id) => {
    if (params[w[id]] !== undefined && document.activeElement !== $(id)) $(id).value = params[w[id]];
  });
}

// ---------------------------------------------------------------- réseau

async function loadConfig() {
  let c;
  try {
    c = await getJson('/config');
  } catch (e) {
    return;
  }
  const f = $('form-net');
  NET_KEYS.forEach((k) => {
    const e = f.elements[k];
    if (!e || c[k] === undefined) return;
    if (e.type === 'checkbox') e.checked = (Number(c[k]) === 1);
    else e.value = c[k];
  });
  f.elements.mqtt_pass.value = '';
}

async function saveNet(ev) {
  ev.preventDefault();
  if (!confirm("Enregistrer la configuration réseau et redémarrer l'ESP ?")) return;
  const f = $('form-net');
  const p = {};
  NET_KEYS.forEach((k) => {
    const e = f.elements[k];
    p[k] = (e.type === 'checkbox') ? (e.checked ? '1' : '0') : e.value.trim();
  });
  // mot de passe vide : conservé côté ESP
  p.mqtt_pass = f.elements.mqtt_pass.value;
  if (await post('/config', p)) show("Configuration enregistrée, redémarrage de l'ESP...", true);
}

// ---------------------------------------------------------------- étalonnage

function done(id) { $(id).classList.add('done'); }

async function wStart() {
  if (!confirm('Commencer : remise à zéro des index et redémarrage du routeur. Continuer ?')) return;
  if (await seq([['indx0', 'PRESS'], ['resetecopv', 'PRESS'], ['v_calib', '0.800']])) {
    done('step1');
    loadParams();
  }
}

async function wGains() {
  const r = Number($('w-p_resistance').value);
  if (!(r > 0)) { show('Puissance du chauffe-eau invalide', false); return; }
  const gp = Math.trunc(5 * 3000.0 / r) + 1, gi = Math.trunc(30 * 3000.0 / r) + 1;
  if (await seq([['p_resistance', r], ['gain_p', gp], ['gain_i', gi], ['e_reserve', 5], ['p_margin', 10]])) done('step3');
}

async function wPCalib() {
  await refresh();
  const papp = Number(status.papp);
  const input = Number($('w-papp').value);
  if (!(papp > 0)) { show('Puissance apparente mesurée nulle : allumez des appareils et vérifiez la pince', false); return; }
  if (!(input > 0)) { show('Puissance apparente du compteur invalide', false); return; }
  await loadParams();
  const cur = Number(params.p_calib);
  if (!(cur > 0)) { show('P_CALIB actuel invalide', false); return; }
  const v = (cur * input / papp).toFixed(4);
  if (await cmd('p_calib', v)) done('step5');
}

async function wOffset() {
  await refresh();
  await loadParams();
  const v = Math.round(Number(params.p_offset) + Number(status.pact));
  if (isNaN(v)) { show('Mesure indisponible', false); return; }
  if (await cmd('p_offset', v)) done('step6');
}

async function wRelay2() {
  const r = Number($('w-p_resistance').value);
  let sec = Number($('w-psec').value);
  if (sec === 0) sec = 500;
  let act, idle;
  if (r > sec) {
    act = Math.trunc(sec * 0.80);
    idle = Math.trunc(sec * 0.30);
  } else {
    act = Math.trunc(r * 0.90);
    idle = Math.trunc(sec - r * 0.70);
  }
  if (await seq([['t_div2_on', 5], ['t_div2_off', 5], ['t_div2_tc', 1], ['p_div2_active', act], ['p_div2_idle', idle]])) done('step8');
}

async function wSave() {
  if (await seq([['savecfg', 'PRESS'], ['resetecopv', 'PRESS']])) {
    done('step10');
    show("Paramètres sauvegardés : MaxPV! est opérationnel, retrouvez le fonctionnement dans l'onglet État.", true);
  }
}

function buildWizard() {
  $('w-start').addEventListener('click', wStart);
  $('w-send-installpv').addEventListener('click', async () => {
    if (await cmd('p_installpv', $('w-p_installpv').value)) done('step2');
  });
  $('w-send-resistance').addEventListener('click', wGains);
  $('w-send-pcalib').addEventListener('click', wPCalib);
  $('w-offset').addEventListener('click', wOffset);
  $('w-phase_calib').addEventListener('change', async (e) => {
    if (await cmd('phase_calib', e.target.value)) done('step7');
  });
  $('w-send-relay2').addEventListener('click', wRelay2);
  $('w-send-cnt').addEventListener('click', async () => {
    if (await cmd('cnt_calib', $('w-cnt_calib').value)) done('step9');
  });
  $('w-save').addEventListener('click', wSave);
}

// ---------------------------------------------------------------- système

async function checkLatest() {
  const out = $('latest-msg');
  out.textContent = 'Recherche de mise à jour...';
  let web = '-';
  try {
    web = (await (await fetch('versionweb.txt', { cache: 'no-store' })).text()).trim();
    $('v-web').textContent = web;
    const r = await fetch(LATEST_URL, { cache: 'no-store' });
    if (!r.ok) throw new Error(String(r.status));
    const l = await r.json();
    let msg = '';
    if (l.ecopvver !== status.ecopv_version) msg += "Le firmware de l'Arduino Nano peut être mis à jour vers " + l.ecopvver + '\n';
    if (l.maxpvver !== status.maxpv_version) msg += 'Le firmware du Wemos peut être mis à jour vers ' + l.maxpvver + '\n';
    if (l.webver !== web) msg += 'Le site Web peut être mis à jour vers ' + l.webver + '\n';
    out.textContent = msg ? msg + '\n' + l.whatsnew : 'Le système est à jour !';
  } catch (e) {
    out.textContent = 'Recherche impossible (pas d\'accès à Internet ?)';
  }
}

async function restore(ev) {
  const file = ev.target.files[0];
  ev.target.value = '';
  if (!file) return;
  let c;
  try {
    c = JSON.parse(await file.text());
  } catch (e) {
    show('Fichier de sauvegarde invalide', false);
    return;
  }
  const keys = NET_KEYS.concat(['boost_duration', 'boost_ratio', 'max_temp', 'temp_limit_active']);
  const p = {};
  keys.forEach((k) => { if (c[k] !== undefined && c[k] !== null) p[k] = String(c[k]); });
  if (!confirm("Restaurer cette configuration et redémarrer l'ESP ?")) return;
  await post('/config', p);
}

function buildActions() {
  document.querySelectorAll('button[data-cmd]').forEach((b) => {
    b.addEventListener('click', async () => {
      const q = b.getAttribute('data-confirm');
      if (q && !confirm(q)) return;
      const ok = await cmd(b.getAttribute('data-cmd'), 'PRESS');
      if (ok && b.getAttribute('data-cmd') === 'loadcfg') loadParams();
    });
  });
}

// ---------------------------------------------------------------- démarrage

document.addEventListener('DOMContentLoaded', () => {
  buildParams();
  buildWizard();
  buildActions();
  $('sel-triac_mode').addEventListener('change', (e) => cmd('triac_mode', e.target.value));
  $('sel-relay_mode').addEventListener('change', (e) => cmd('relay_mode', e.target.value));
  $('btn-boost').addEventListener('click', async () => {
    if (await cmd('boost', boostOn ? 'OFF' : 'ON')) refresh();
  });
  $('chk-temp_limit').addEventListener('change', (e) => cmd('temp_limit', e.target.checked ? 'ON' : 'OFF'));
  $('form-net').addEventListener('submit', saveNet);
  $('file-restore').addEventListener('change', restore);
  $('btn-latest').addEventListener('click', checkLatest);
  window.addEventListener('hashchange', showTab);
  fetch('versionweb.txt').then((r) => r.text()).then((t) => { $('v-web').textContent = t.trim(); }).catch(() => {});
  showTab();
});
