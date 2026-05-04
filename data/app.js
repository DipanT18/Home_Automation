/**
 * app.js — Home Automation Dashboard
 * Polls /api/temperature and /api/status every 10 s and updates the UI.
 */

'use strict';

const REFRESH_INTERVAL = 10; // seconds between auto-refreshes
let countdownValue = REFRESH_INTERVAL;
let countdownTimer = null;

// ── DOM helpers ──────────────────────────────────────────────────────────────
function el(id) {
  return document.getElementById(id);
}

function setText(id, text) {
  const node = el(id);
  if (node) node.textContent = text;
}

// ── Data fetch ───────────────────────────────────────────────────────────────
async function fetchData() {
  try {
    const [tempResp, statusResp] = await Promise.all([
      fetch('/api/temperature'),
      fetch('/api/status'),
    ]);

    if (!tempResp.ok || !statusResp.ok) {
      throw new Error('HTTP error from ESP32');
    }

    const temp   = await tempResp.json();
    const status = await statusResp.json();

    updateTemperature(temp);
    updateStatus(status);

    const now = new Date();
    setText('last-updated', now.toLocaleTimeString());
  } catch (err) {
    console.error('[HomeAuto] Fetch error:', err);
    setText('temperature',   'ERR');
    setText('humidity',      'ERR');
    setText('temperature-f', 'Could not reach ESP32');
  }
}

// ── UI updaters ──────────────────────────────────────────────────────────────
function updateTemperature(data) {
  if (data.error) {
    setText('temperature',   'ERR');
    setText('humidity',      'ERR');
    setText('temperature-f', data.error);
    return;
  }

  const tC = parseFloat(data.temperature_c);
  const tF = parseFloat(data.temperature_f);
  const rh = parseFloat(data.humidity);

  setText('temperature',   isFinite(tC) ? tC.toFixed(1) : '--');
  setText('temperature-f', isFinite(tF) ? tF.toFixed(1) + ' °F' : '--');
  setText('humidity',      isFinite(rh) ? rh.toFixed(1) : '--');
}

function updateStatus(data) {
  setText('ip',       data.ip       || '--');
  setText('hostname', data.hostname  || '--');
  setText('rssi',     data.rssi != null ? data.rssi + ' dBm' : '--');
  setText('uptime',   data.uptime_s  != null ? formatUptime(data.uptime_s) : '--');
  setText('heap',     data.heap_free != null ? formatBytes(data.heap_free)  : '--');
}

// ── Formatters ───────────────────────────────────────────────────────────────
function formatUptime(seconds) {
  const s = Math.floor(seconds);
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  return `${h}h ${m}m ${sec}s`;
}

function formatBytes(bytes) {
  if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + ' MB';
  if (bytes >= 1024)    return (bytes / 1024).toFixed(1)    + ' KB';
  return bytes + ' B';
}

// ── Countdown ────────────────────────────────────────────────────────────────
function startCountdown() {
  clearInterval(countdownTimer);
  countdownValue = REFRESH_INTERVAL;
  setText('countdown', countdownValue);

  countdownTimer = setInterval(() => {
    countdownValue -= 1;
    setText('countdown', countdownValue);
    if (countdownValue <= 0) {
      refresh();
    }
  }, 1000);
}

// ── Public: called by button and auto-refresh ────────────────────────────────
function refresh() {   // eslint-disable-line no-unused-vars
  fetchData();
  startCountdown();
}

// ── Init ─────────────────────────────────────────────────────────────────────
refresh();
