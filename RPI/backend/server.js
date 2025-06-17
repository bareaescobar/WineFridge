const express = require('express');
const { createServer: createViteServer } = require('vite');
const mqtt = require('mqtt');
const WebSocket = require('ws');
const http = require('http');

const PORT = 3000;
const MQTT_URL = 'mqtt://localhost:1883';

async function startServer() {
  const app = express();
  const server = http.createServer(app);

  // Setup WebSocket server
  const wss = new WebSocket.Server({ server });

  // Development or production?
  const isDev = process.env.NODE_ENV !== 'production';

  if (isDev) {
    const vite = await createViteServer({
      server: { middlewareMode: true }
    });
    app.use(vite.middlewares);
  } else {
    app.use(express.static('dist'));
  }

  // MQTT Client Setup
  const mqttClient = mqtt.connect(MQTT_URL);

  mqttClient.on('connect', () => {
    console.log('[MQTT] Connected');
    mqttClient.subscribe('winefridge/system/status');
  });

  mqttClient.on('message', (topic, message) => {
    const msg = message.toString();
    wss.clients.forEach(client => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(msg);
      }
    });
  });

  // Handle WebSocket connections from frontend
  wss.on('connection', (ws) => {
    console.log('[WebSocket] Web client connected');

    ws.on('message', (data) => {
      try {
        const message = JSON.parse(data);
        mqttClient.publish('winefridge/system/command', JSON.stringify(message));
      } catch (err) {
        console.error('[WebSocket] Invalid JSON:', err);
      }
    });
  });

  server.listen(PORT, () => {
    console.log(`[Web] Server running at http://localhost:${PORT}`);
  });
}

startServer();
