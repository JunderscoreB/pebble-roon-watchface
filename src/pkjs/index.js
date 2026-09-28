/*
 * Pebble Roon Remote
 * Copyright (c) 2026 J_B
 *
 * Released under the MIT License.
 *
 * AI Disclosure: Portions of this file were generated and optimized with the assistance of generative AI.
 * Co-Authored-By: Google Gemini <noreply@google.com>
 */

var Clay = require('@rebble/clay');
var clayConfig = require('./config.js');

var customClayFile = null;
var devConfig = {};

var customClay = new Clay(clayConfig, customClayFile, { autoHandleEvents: false });

var DEFAULT_IP = devConfig.ip || "192.168.1.50";
var DEFAULT_PORT = "3000";
var CONFIG_URL = "https://junderscoreb.github.io/pebble-roon-remote/config.html";

var g_isPlaying = false;
var g_messageQueue = [];
var g_isSendingMessage = false;
var g_pollTimer = null;
var g_isConfiguring = false;
var g_lastCommandTime = 0; 

function getBridgeUrl() {
  var ip = localStorage.getItem('bridge_ip') || DEFAULT_IP;
  var port = localStorage.getItem('bridge_port') || DEFAULT_PORT;

  ip = ip.trim().replace(/^https?:\/\//, '').replace(/\/+$/, '');
  port = port.trim();
  return "http://" + ip + ":" + port + "/";
}

function sendAppMessageQueue(dictionary) {
  g_messageQueue.push(dictionary);
  pumpQueue();
}

function pumpQueue() {
  if (g_isSendingMessage || g_messageQueue.length === 0) return;
  g_isSendingMessage = true;
  var dict = g_messageQueue[0];

  Pebble.sendAppMessage(dict,
    function(e) {
      g_messageQueue.shift();
      g_isSendingMessage = false;
      pumpQueue();
    },
    function(e) {
      g_isSendingMessage = false;
      setTimeout(pumpQueue, 100);
    }
  );
}

function sendBridgeCommand(command) {
  var req = new XMLHttpRequest();
  var url = getBridgeUrl() + command;
  console.log("[Roon Remote] TX Command: " + url);

  req.open('GET', url, true);
  req.setRequestHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  req.setRequestHeader("Pragma", "no-cache");
  req.setRequestHeader("Expires", "0");

  req.onload = function() {
    if (req.status === 200) {
      sendToWatch(req.responseText);
      if (command !== 'status' && command !== 'launch') {
        setTimeout(fetchStatus, 350);
      }
    } else {
      console.log("[Roon Remote] HTTP Error " + req.status + " on " + command);
    }
  };
  req.send(null);
}

function scheduleNextFetch() {
  if (g_pollTimer) clearTimeout(g_pollTimer);
  // Adaptive polling: 3s if active, 30s if paused to conserve watch battery
  var pollInterval = g_isPlaying ? 3000 : 30000;
  g_pollTimer = setTimeout(fetchStatus, pollInterval);
}

function fetchStatus(isLaunch) {
  var endpoint = (isLaunch === true) ? 'launch' : 'status';
  var req = new XMLHttpRequest();
  var url = getBridgeUrl() + endpoint;

  req.open('GET', url, true);
  req.setRequestHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  req.setRequestHeader("Pragma", "no-cache");
  req.setRequestHeader("Expires", "0");

  req.onload = function() {
    if (req.status === 200) {
      sendToWatch(req.responseText);
      scheduleNextFetch();
    } else {
      console.log("[Roon Remote] HTTP Error " + req.status + " on " + endpoint);
      sendErrorToWatch();
      scheduleNextFetch();
    }
  };
  req.onerror = function() {
    console.log("[Roon Remote] Network connection failed on " + endpoint);
    sendErrorToWatch();
    scheduleNextFetch();
  };
  req.ontimeout = function() {
    console.log("[Roon Remote] Request timed out on " + endpoint);
    sendErrorToWatch();
    scheduleNextFetch();
  };
  req.timeout = 4000;
  req.send(null);
}

function getBasePayload() {
  var rawFont = localStorage.getItem('font_size');
  var savedFont = (rawFont === 'large' || rawFont === '2') ? 2 : (rawFont === 'small' || rawFont === '0') ? 0 : 1;
  
  var rawScroll = localStorage.getItem('scroll_text');
  var isScrollEnabled = (rawScroll === 'true' || rawScroll === '1' || rawScroll === true) ? 1 : 0;
  
  var rawTouch = localStorage.getItem('enable_touch');
  var isTouchEnabled = 1;
  if (rawTouch === 'false' || rawTouch === '0' || rawTouch === false || rawTouch === "") {
    isTouchEnabled = 0;
  }

  var rawQuiet = localStorage.getItem('respect_quiet_time');
  var isQuietEnabled = 1; 
  if (rawQuiet === 'false' || rawQuiet === '0' || rawQuiet === false || rawQuiet === "") {
    isQuietEnabled = 0;
  }

  var rawTheme = localStorage.getItem('theme');
  var isLightMode = (rawTheme === 'true' || rawTheme === '1' || rawTheme === true) ? 1 : 0;
  
  var timeApp = parseInt(localStorage.getItem('timeout_app') || '0', 10);
  var timeDisc = parseInt(localStorage.getItem('timeout_disc') || '0', 10);

  return {
    'font_size': savedFont,
    'scroll_text': isScrollEnabled,
    'timeout_app': timeApp,
    'timeout_disc': timeDisc,
    'enable_touch': isTouchEnabled,
    'respect_quiet_time': isQuietEnabled,
    'theme': isLightMode,
    'is_configuring': g_isConfiguring ? 1 : 0
  };
}

function sendErrorToWatch() {
  var payload = getBasePayload();
  payload['error'] = 1;
  sendAppMessageQueue(payload);
}

function sendConfigToWatch() {
  var payload = getBasePayload();
  payload['error'] = 0;
  sendAppMessageQueue(payload);
}

function sendToWatch(responseText) {
  try {
    var response = JSON.parse(responseText);
    var payload = getBasePayload();

    if (response.is_playing !== undefined) g_isPlaying = response.is_playing;

    var safeVolume = -1;
    var isFixed = (response.is_fixed_volume === true); 
    
    if (response.volume !== undefined && response.volume !== null) {
      if (typeof response.volume === 'object') {
        safeVolume = parseInt(response.volume.value, 10);
        if (response.volume.type === 'fixed') isFixed = true;
      } else { 
        safeVolume = parseInt(response.volume, 10); 
      }
    } else if (response.volume_value !== undefined && response.volume_value !== null) {
      safeVolume = parseInt(response.volume_value, 10);
    } else if (response.level !== undefined && response.level !== null) {
      safeVolume = parseInt(response.level, 10);
    }

    if (isNaN(safeVolume)) safeVolume = -1;
    if (safeVolume !== -1 && safeVolume !== null) isFixed = false;

    payload['zone_name'] = response.zone || "Unknown";
    payload['track'] = response.track || "";
    payload['artist'] = response.artist || "";
    payload['is_playing'] = response.is_playing ? 1 : 0;
    payload['volume_val'] = safeVolume;
    payload['is_fixed'] = isFixed ? 1 : 0;
    payload['error'] = 0;

    sendAppMessageQueue(payload);
  } catch (err) { console.log("[Roon Remote] JSON Parse Error: " + err); }
}

Pebble.addEventListener('ready', function() {
  fetchStatus(true);
});

Pebble.addEventListener('appmessage', function(e) {
  var command = e.payload['command'] || e.payload['KEY_COMMAND'] || e.payload['0'] || e.payload[0];
  
  if (command === "retry_connection" || command === "status") {
    fetchStatus(true);
    return;
  }

  var now = Date.now();
  if ((command === "next" || command === "previous" || command === "playpause" || command === "next_zone" || command === "prev_zone") && (now - g_lastCommandTime < 500)) {
    console.log("[Roon Remote] Dropping debounced command: " + command);
    return;
  }
  g_lastCommandTime = now;

  if (command === "playpause") {
    sendBridgeCommand(command);
  } else if ((command === "next" || command === "previous") && !g_isPlaying) {
    sendBridgeCommand(command);
    setTimeout(function() { sendBridgeCommand("pause"); }, 2500);
  } else if (command) {
    sendBridgeCommand(command);
  }
});

Pebble.addEventListener('showConfiguration', function(e) {
  g_isConfiguring = true;
  sendAppMessageQueue({ 'is_configuring': 1 });

  var watchInfo = null;
  if (typeof Pebble.getActiveWatchInfo === 'function') {
    watchInfo = Pebble.getActiveWatchInfo();
  }
  var platform = watchInfo ? watchInfo.platform : 'aplite';

  if (platform === 'aplite' || platform === 'gabbro' || platform === 'flint') {
    var ip = localStorage.getItem('bridge_ip') || DEFAULT_IP;
    var port = localStorage.getItem('bridge_port') || DEFAULT_PORT;
    var rawFont = localStorage.getItem('font_size') || "1";
    var scrollText = localStorage.getItem('scroll_text') || '0';
    var timeApp = localStorage.getItem('timeout_app') || '0';
    var timeDisc = localStorage.getItem('timeout_disc') || '0';
    var enableTouch = localStorage.getItem('enable_touch') || '1';
    var respectQuietTime = localStorage.getItem('respect_quiet_time') || '1';
    var theme = localStorage.getItem('theme') || '0';

    var cacheBuster = Math.round(Math.random() * 10000);

    var finalUrl = CONFIG_URL + "?v=" + cacheBuster +
    "&ip=" + encodeURIComponent(ip) +
    "&port=" + encodeURIComponent(port) +
    "&font_size=" + encodeURIComponent(rawFont) +
    "&scroll_text=" + encodeURIComponent(scrollText) +
    "&timeout_app=" + encodeURIComponent(timeApp) +
    "&timeout_disc=" + encodeURIComponent(timeDisc) +
    "&enable_touch=" + encodeURIComponent(enableTouch) +
    "&respect_quiet_time=" + encodeURIComponent(respectQuietTime) +
    "&theme=" + encodeURIComponent(theme);

    Pebble.openURL(finalUrl);
  } else {
    Pebble.openURL(customClay.generateUrl());
  }
});

Pebble.addEventListener('webviewclosed', function(e) {
  g_isConfiguring = false;
  if (!e || !e.response || e.response === "CANCELLED") {
    sendAppMessageQueue({ 'is_configuring': 0 });
    return;
  }

  try {
    var watchInfo = null;
    if (typeof Pebble.getActiveWatchInfo === 'function') {
      watchInfo = Pebble.getActiveWatchInfo();
    }
    var platform = watchInfo ? watchInfo.platform : 'aplite';

    if (platform !== 'aplite' && platform !== 'gabbro' && platform !== 'flint') {
      try { customClay.getSettings(e.response, false); } catch(ex) {}
    }

    var responseDict = JSON.parse(decodeURIComponent(e.response));

    function extractVal(key) {
      if (responseDict[key] !== undefined) {
        if (typeof responseDict[key] === 'object' && responseDict[key] !== null && 'value' in responseDict[key]) {
          return responseDict[key].value.toString();
        }
        return responseDict[key].toString();
      }
      return null;
    }

    var ip = extractVal('bridge_ip') || extractVal('ip');
    var port = extractVal('bridge_port') || extractVal('port');
    var font_size = extractVal('font_size');
    var scroll_text = extractVal('scroll_text');
    var timeout_app = extractVal('timeout_app');
    var timeout_disc = extractVal('timeout_disc');
    var enable_touch = extractVal('enable_touch');
    var respect_quiet_time = extractVal('respect_quiet_time');
    var theme = extractVal('theme');

    if (ip) localStorage.setItem('bridge_ip', ip);
    if (port) localStorage.setItem('bridge_port', port);
    if (font_size) localStorage.setItem('font_size', font_size);
    if (scroll_text !== null) localStorage.setItem('scroll_text', scroll_text);
    if (timeout_app) localStorage.setItem('timeout_app', timeout_app);
    if (timeout_disc) localStorage.setItem('timeout_disc', timeout_disc);
    if (enable_touch !== null) localStorage.setItem('enable_touch', enable_touch);
    if (respect_quiet_time !== null) localStorage.setItem('respect_quiet_time', respect_quiet_time);
    if (theme !== null) localStorage.setItem('theme', theme);

    sendConfigToWatch();
    fetchStatus(true);

  } catch(err) {
    console.log("[Roon Remote] Error parsing settings: " + err);
  }
});