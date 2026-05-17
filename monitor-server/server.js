const express = require('express');
const mqtt = require('mqtt');

const BROKER = 'mqtt://broker.emqx.io';
const TOPIC = 'soundsystem/10B41D655F68/assembly';
const PORT = 3000;

const app = express();
const clients = new Set();
let lastMessage = null;

const mqttClient = mqtt.connect(BROKER);

mqttClient.on('connect', () => {
  console.log(`Connected to ${BROKER}`);
  mqttClient.subscribe(TOPIC, (err) => {
    if (err) console.error('Subscribe error:', err);
    else console.log(`Subscribed to ${TOPIC}`);
  });
});

mqttClient.on('message', (topic, message, packet) => {
  const raw = message.toString();
  let payload;
  try { payload = JSON.parse(raw); } catch (_) { payload = raw; }
  const retained = !!packet.retain;
  console.log(`[${topic}]${retained ? ' (retained)' : ''}`, payload);
  lastMessage = { topic, payload, ts: Date.now(), retained };
  const data = `data: ${JSON.stringify(lastMessage)}\n\n`;
  for (const res of clients) res.write(data);
});

mqttClient.on('error', (err) => console.error('MQTT error:', err));

app.get('/events', (req, res) => {
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');
  res.flushHeaders();
  if (lastMessage) res.write(`data: ${JSON.stringify(lastMessage)}\n\n`);
  clients.add(res);
  req.on('close', () => clients.delete(res));
});

app.get('/', (req, res) => {
  res.send(`<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Sound System Monitor</title>
  <style>
    * { box-sizing: border-box; }
    body { font-family: monospace; background: #111; color: #0f0; margin: 0; padding: 0; height: 100vh; display: flex; flex-direction: column; }
    header { padding: 0.5rem 1rem; background: #1a1a1a; border-bottom: 1px solid #333; color: #fff; font-size: 0.85rem; }
    header span { color: #555; margin-left: 1rem; }
    .layout { display: flex; flex: 1; overflow: hidden; }
    .panel-left { width: 320px; min-width: 200px; border-right: 1px solid #333; display: flex; flex-direction: column; overflow: hidden; }
    .panel-left h2 { margin: 0; padding: 0.4rem 0.75rem; font-size: 0.75rem; color: #555; background: #161616; border-bottom: 1px solid #222; }
    #log { list-style: none; padding: 0; margin: 0; overflow-y: auto; flex: 1; }
    #log li { padding: 0.4rem 0.75rem; border-bottom: 1px solid #1e1e1e; cursor: pointer; transition: background 0.1s; }
    #log li:hover { background: #1e1e1e; }
    #log li.active { background: #1a2a1a; border-left: 2px solid #0f0; }
    .log-ts { color: #555; font-size: 0.75rem; display: block; }
    .log-info { color: #0a0; font-size: 0.8rem; margin-top: 0.15rem; }
    .log-err { color: red; font-size: 0.8rem; }
    .panel-right { flex: 1; display: flex; flex-direction: column; overflow: hidden; }
    .panel-right h2 { margin: 0; padding: 0.4rem 0.75rem; font-size: 0.75rem; color: #555; background: #161616; border-bottom: 1px solid #222; }
    #json-view { flex: 1; width: 100%; border: none; outline: none; background: #111; color: #0f0; font-family: monospace; font-size: 0.85rem; padding: 0.75rem; resize: none; }
  </style>
</head>
<body>
  <header>MQTT Monitor <span>${TOPIC}</span></header>
  <div class="layout">
    <div class="panel-left">
      <h2>Log</h2>
      <ul id="log"></ul>
    </div>
    <div class="panel-right">
      <h2>JSON</h2>
      <textarea id="json-view" readonly></textarea>
    </div>
  </div>
  <script>
    const es = new EventSource('/events');
    const log = document.getElementById('log');
    const jsonView = document.getElementById('json-view');
    let activeItem = null;

    function countElements(obj) {
      if (Array.isArray(obj)) return obj.length + ' items';
      if (obj && typeof obj === 'object') return Object.keys(obj).length + ' keys';
      return typeof obj;
    }

    function setActive(li, pretty) {
      if (activeItem) activeItem.classList.remove('active');
      activeItem = li;
      li.classList.add('active');
      jsonView.value = pretty;
    }

    es.onmessage = (e) => {
      const { payload, ts, retained } = JSON.parse(e.data);
      const pretty = JSON.stringify(payload, null, 2);
      const byteLen = new TextEncoder().encode(pretty).length;
      const info = countElements(payload) + ' · ' + byteLen + ' B';

      const li = document.createElement('li');
      const tsSpan = document.createElement('span');
      tsSpan.className = 'log-ts';
      tsSpan.textContent = new Date(ts).toLocaleTimeString() + (retained ? ' · retained' : '');
      const infoSpan = document.createElement('span');
      infoSpan.className = 'log-info';
      infoSpan.textContent = info;
      li.appendChild(tsSpan);
      li.appendChild(infoSpan);
      li.addEventListener('click', () => setActive(li, pretty));
      log.prepend(li);
      setActive(li, pretty);
    };

    es.onerror = () => {
      const li = document.createElement('li');
      li.innerHTML = '<span class="log-err">Connection lost, retrying…</span>';
      log.prepend(li);
    };
  </script>
</body>
</html>`);
});

app.listen(PORT, () => console.log(`Server listening on http://localhost:${PORT}`));
