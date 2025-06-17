module.exports = {
  apps: [
    {
      name: 'mqtt-handler',
      script: 'mqtt_handler.py',
      interpreter: 'python3'
    },
    {
      name: 'barcode-scanner',
      script: 'barcode_scanner.py',
      interpreter: 'python3'
    },
    {
      name: 'web-server',
      script: 'server.js'
    }
  ]
};
