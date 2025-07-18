module.exports = {
  apps: [
    {
      name: 'mqtt-handler',
      script: 'mqtt_handler.py',
      interpreter: 'python3',
      cwd: './backend'
    },
    {
      name: 'barcode-scanner',
      script: 'barcode_scanner.py',
      interpreter: 'python3',
      cwd: './backend'
    },
    {
      name: 'web-server',
	    script: 'server.cjs',
      cwd: './frontend/backend'
    },
    {
      name: 'kiosk',
      script: 'chromium-browser',
      args: '--kiosk --noerrdialogs --disable-infobars --incognito --app=http://localhost:3000 --ozone-platform=wayland --disable-features=WaylandWindowDecorations --log-level=3',
      exec_mode: 'fork',
      interpreter: 'none',
      env: {
        XDG_SESSION_TYPE: 'wayland',
        XDG_CURRENT_DESKTOP: 'LXDE'
      }
    }
  ]
};
