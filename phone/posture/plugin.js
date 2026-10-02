import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';
import {
  SETTINGS_CHANNEL,
  EVENT_CHANNEL,
  EVENT_CALIBRATED,
  EVENT_ALERT,
  DEFAULT_THRESHOLD_DEG,
  DEFAULT_DURATION_SEC,
  decodeEvent,
  encodeConfig,
  encodeRequestCalibrate,
} from './protocol.js';

const STORAGE_KEY = 'posture';

const gm = createGMPlugin();

const statusEl = document.querySelector('#status');
const enableToggle = document.querySelector('#enable-toggle');
const thresholdInput = document.querySelector('#threshold');
const thresholdValue = document.querySelector('#threshold-value');
const durationInput = document.querySelector('#duration');
const durationValue = document.querySelector('#duration-value');
const calibrateButton = document.querySelector('#calibrate-button');
const todayCountEl = document.querySelector('#today-count');
const weekCountEl = document.querySelector('#week-count');
const lastAlertEl = document.querySelector('#last-alert');

let config = {
  enabled: false,
  threshold: DEFAULT_THRESHOLD_DEG,
  durationSec: DEFAULT_DURATION_SEC,
};
let history = {}; // { 'YYYY-MM-DD': count }
let connected = false;

function setStatus(text, state = '') {
  statusEl.textContent = text;
  statusEl.className = `status ${state}`.trim();
}

function todayKey() {
  const now = new Date();
  const month = String(now.getMonth() + 1).padStart(2, '0');
  const day = String(now.getDate()).padStart(2, '0');
  return `${now.getFullYear()}-${month}-${day}`;
}

function formatTime(iso) {
  const date = new Date(iso);
  return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}

async function loadState() {
  const result = await gm.storage.get(STORAGE_KEY);
  if (result.value) {
    if (result.value.config) config = { ...config, ...result.value.config };
    if (result.value.history) history = result.value.history;
  }
}

async function persistState() {
  await gm.storage.set(STORAGE_KEY, { config, history });
}

function renderStats() {
  const today = history[todayKey()] || 0;
  let week = 0;
  const now = new Date();
  for (let i = 0; i < 7; i += 1) {
    const d = new Date(now);
    d.setDate(now.getDate() - i);
    const month = String(d.getMonth() + 1).padStart(2, '0');
    const day = String(d.getDate()).padStart(2, '0');
    week += history[`${d.getFullYear()}-${month}-${day}`] || 0;
  }
  todayCountEl.textContent = String(today);
  weekCountEl.textContent = String(week);
}

function renderConfig() {
  enableToggle.checked = config.enabled;
  thresholdInput.value = String(config.threshold);
  thresholdValue.textContent = String(config.threshold);
  durationInput.value = String(config.durationSec);
  durationValue.textContent = String(config.durationSec);
  thresholdInput.disabled = !connected;
  durationInput.disabled = !connected;
  calibrateButton.disabled = !connected;
}

async function sendConfig() {
  if (!connected) return;
  try {
    await gm.plugin.sendMessage(SETTINGS_CHANNEL, encodeConfig(config));
  } catch (error) {
    setStatus(error.message || String(error), 'error');
  }
}

async function onConfigChanged() {
  await persistState();
  renderConfig();
  await sendConfig();
}

enableToggle.addEventListener('change', () => {
  config.enabled = enableToggle.checked;
  void onConfigChanged();
});

thresholdInput.addEventListener('input', () => {
  config.threshold = Number(thresholdInput.value);
  thresholdValue.textContent = thresholdInput.value;
  void onConfigChanged();
});

durationInput.addEventListener('input', () => {
  config.durationSec = Number(durationInput.value);
  durationValue.textContent = durationInput.value;
  void onConfigChanged();
});

calibrateButton.addEventListener('click', async () => {
  if (!connected) return;
  try {
    await gm.plugin.sendMessage(SETTINGS_CHANNEL, encodeRequestCalibrate());
    lastAlertEl.textContent = 'Calibration demandée…';
  } catch (error) {
    setStatus(error.message || String(error), 'error');
  }
});

const offMessages = gm.plugin.onMessage((message) => {
  if (message.channel !== EVENT_CHANNEL) return;
  const decoded = decodeEvent(message.data);
  if (!decoded) return;
  if (decoded.event === EVENT_CALIBRATED) {
    lastAlertEl.textContent = `Posture de référence calibrée (${decoded.baseline}°).`;
    return;
  }
  if (decoded.event === EVENT_ALERT) {
    const key = todayKey();
    history[key] = (history[key] || 0) + 1;
    void persistState();
    renderStats();
    lastAlertEl.textContent = `Dernier rappel à ${formatTime(new Date().toISOString())} (angle ${decoded.pitch}°).`;
  }
});

async function start() {
  try {
    await gm.ready();
    await loadState();
    renderStats();
    renderConfig();
    const info = await gm.device.getInfo();
    connected = Boolean(info.connected);
    setStatus(connected ? 'Lunettes connectées' : 'Lunettes déconnectées', connected ? 'ready' : 'error');
    renderConfig();
    // Push the saved settings so the glasses resume the chosen behavior.
    await sendConfig();
    await gm.device.subscribeEvents(['connection']);
    gm.device.onConnection((event) => {
      connected = Boolean(event.connected);
      setStatus(connected ? 'Lunettes connectées' : 'Lunettes déconnectées', connected ? 'ready' : 'error');
      renderConfig();
      if (connected) void sendConfig();
    });
  } catch (error) {
    setStatus(error.message || String(error), 'error');
  }
}

window.addEventListener('pagehide', () => {
  offMessages();
  gm.close();
});

void start();
