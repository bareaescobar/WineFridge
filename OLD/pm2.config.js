module.exports = {
  apps: [
    {
      name: 'mqtt-handler',
      script: 'mqtt_handler.py',
      interpreter: '/usr/bin/python3',
      cwd: './backend'
    },
    {
      name: 'barcode-scanner',
      script: 'barcode_scanner.py',
      interpreter: '/usr/bin/python3',
      cwd: './backend'
    },
    {
      name: 'web-server',
      script: 'server.cjs',
      cwd: './frontend/backend',
      interpreter: 'node',
      env: {
        NODE_ENV: 'production'
      }
    },
    {
      name: 'kiosk',
      script: 'chromium-browser',
      args: '--kiosk --noerrdialogs --disable-infobars --disable-session-crashed-bubble --disable-translate --incognito --disable-cache --app=http://localhost:3000 --ozone-platform=wayland --disable-features=WaylandWindowDecorations --log-level=3',
      exec_mode: 'fork',
      interpreter: 'none',
      env: {
        XDG_SESSION_TYPE: 'wayland',
        XDG_CURRENT_DESKTOP: 'LXDE'
      }
    }
  ]
};
