import mqtt from 'mqtt'

let client = null
const reconnectPeriod = 5000 // 5 seconds

export function connectMQTT({ host, options = {} } = {}) {
  if (client && client.connected) return client
  client = mqtt.connect(host, { reconnectPeriod, ...options })
  client.on('connect', () => {
    console.log('[MQTT] Connected to broker')
  })
  client.on('error', (err) => {
    console.error('[MQTT] Connection error:', err)
    client.end()
  })
  client.on('close', () => {
    console.log('[MQTT] Connection closed')
  })
  return client
}

export function subscribe(topic, callback) {
  if (!client) throw new Error('MQTT client not connected')
  client.subscribe(topic, (err) => {
    if (err) {
      console.error(`[MQTT] Failed to subscribe to ${topic}:`, err)
    } else {
      console.log(`[MQTT] Subscribed to ${topic}`)
    }
  })
  client.on('message', (receivedTopic, message) => {
    if (receivedTopic === topic) {
      callback(message.toString(), receivedTopic)
    }
  })
}

export function publish(topic, message) {
  if (!client) throw new Error('MQTT client not connected')
  client.publish(topic, message, (err) => {
    if (err) {
      console.error(`[MQTT] Failed to publish to ${topic}:`, err)
    }
  })
}

export function disconnect() {
  if (client) {
    client.end()
    client = null
  }
}
