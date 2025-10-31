/**
 * Lighting Control Module for WineFridge
 * Handles zone and drawer lighting via MQTT
 * 
 * Zone/Drawer/ESP32 Mapping:
 * 
 * LOWER ZONE (Zone 1, Drawers 1-3):
 * - ESP32_LIGHTING2: Zone 1 general, Drawer 1 & 2 general
 * - ESP32_DRAWER3: Drawer 3 general lighting
 * 
 * MIDDLE ZONE (Zone 2, Drawers 4-6):
 * - ESP32_LIGHTING6: Zone 2 general, Drawer 4 & 6 general
 * - ESP32_DRAWER5: Drawer 5 general lighting
 * 
 * UPPER ZONE (Zone 3, Drawers 7-9):
 * - ESP32_LIGHTING8: Zone 3 general, Drawer 8 & 9 general
 * - ESP32_DRAWER7: Drawer 7 general lighting
 */

import { connectMQTT, publish, subscribe } from './mqttClient'
import { TOPICS } from './constants/topics'
import { BROKER_URL } from './constants/mqtt-variables'

// Color temperature presets
const COLOR_TEMPS = {
  WARM: 2700,      // Warm white (2700K)
  NEUTRAL: 6500,   // Neutral white (6500K)
  COOL: 4600       // Cool white (average of warm + neutral)
}

// Zone to ESP32 mapping
const ZONE_ESP_MAPPING = {
  lower: {
    zone_controller: 'lighting_2',
    drawers: {
      drawer_1: 'lighting_2',
      drawer_2: 'lighting_2', 
      drawer_3: 'drawer_3'
    }
  },
  middle: {
    zone_controller: 'lighting_6',
    drawers: {
      drawer_4: 'lighting_6',
      drawer_5: 'drawer_5',
      drawer_6: 'lighting_6'
    }
  },
  upper: {
    zone_controller: 'lighting_8',
    drawers: {
      drawer_7: 'drawer_7',
      drawer_8: 'lighting_8',
      drawer_9: 'lighting_8'
    }
  }
}

/**
 * Convert UI color temperature to Kelvin
 * @param {string} colorType - 'warm', 'neutral', or 'cool'
 * @returns {number} Temperature in Kelvin
 */
function getTemperatureKelvin(colorType) {
  switch(colorType.toLowerCase()) {
    case 'warm':
      return COLOR_TEMPS.WARM
    case 'neutral':
      return COLOR_TEMPS.NEUTRAL
    case 'cool':
      return COLOR_TEMPS.COOL
    default:
      return COLOR_TEMPS.NEUTRAL
  }
}

/**
 * Send zone lighting command
 * @param {string} zone - 'lower', 'middle', or 'upper'
 * @param {number} temperature - Temperature in Kelvin
 * @param {number} brightness - Brightness percentage (0-100)
 */
export function setZoneLighting(zone, temperature, brightness) {
  const zoneConfig = ZONE_ESP_MAPPING[zone.toLowerCase()]
  if (!zoneConfig) {
    console.error(`Invalid zone: ${zone}`)
    return
  }

  // Send to zone controller
  const zoneCommand = {
    action: 'set_zone_light',
    source: 'web',
    data: {
      zone: zone.toLowerCase(),
      temperature: temperature,
      brightness: brightness
    },
    timestamp: new Date().toISOString()
  }

  const zoneTopic = `winefridge/${zoneConfig.zone_controller}/command`
  publish(zoneTopic, JSON.stringify(zoneCommand))
  
  console.log(`[LIGHTING] Set ${zone} zone: ${temperature}K @ ${brightness}%`)
}

/**
 * Send drawer lighting command
 * @param {string} drawer - Drawer ID (e.g., 'drawer_1')
 * @param {number} temperature - Temperature in Kelvin
 * @param {number} brightness - Brightness percentage (0-100)
 */
export function setDrawerLighting(drawer, temperature, brightness) {
  // Find which ESP32 controls this drawer
  let controller = null
  for (const [zoneName, zoneConfig] of Object.entries(ZONE_ESP_MAPPING)) {
    if (zoneConfig.drawers[drawer]) {
      controller = zoneConfig.drawers[drawer]
      break
    }
  }
  
  if (!controller) {
    console.error(`No controller found for drawer: ${drawer}`)
    return
  }

  // Different command based on controller type
  let command
  if (controller.startsWith('lighting_')) {
    // ESP32_LIGHTING controller
    command = {
      action: 'set_drawer_general',
      source: 'web',
      data: {
        drawers: {
          [drawer]: {
            temperature: temperature,
            brightness: brightness
          }
        }
      },
      timestamp: new Date().toISOString()
    }
  } else {
    // ESP32_DRAWER controller (functional drawers)
    command = {
      action: 'set_general_light',
      source: 'web',
      data: {
        temperature: temperature,
        brightness: brightness
      },
      timestamp: new Date().toISOString()
    }
  }

  const topic = `winefridge/${controller}/command`
  publish(topic, JSON.stringify(command))
  
  console.log(`[LIGHTING] Set ${drawer}: ${temperature}K @ ${brightness}%`)
}

/**
 * Apply lighting preset to all zones
 * @param {string} mode - 'energy_saving', 'night_mode', 'normal', 'party'
 */
export function setLightingMode(mode) {
  let temperature, brightness
  
  switch(mode) {
    case 'energy_saving':
      temperature = COLOR_TEMPS.WARM
      brightness = 30
      break
    case 'night_mode':
      temperature = COLOR_TEMPS.WARM
      brightness = 10
      break
    case 'party':
      temperature = COLOR_TEMPS.COOL
      brightness = 100
      break
    default: // normal
      temperature = COLOR_TEMPS.NEUTRAL
      brightness = 70
      break
  }
  
  // Apply to all zones
  ['lower', 'middle', 'upper'].forEach(zone => {
    setZoneLighting(zone, temperature, brightness)
  })
  
  console.log(`[LIGHTING] Applied mode: ${mode}`)
}

/**
 * Handle lighting modal save actions
 */
export function setupLightingHandlers() {
  // Zone lighting modal save
  document.addEventListener('click', (e) => {
    const saveBtn = e.target.closest('.zone-lighting-modal-save')
    if (!saveBtn) return
    
    const form = saveBtn.closest('.form')
    const zone = form.dataset.key
    
    // Get form values
    const brightnessInput = form.querySelector('input[name="brightness"]')
    const colorTypeInputs = form.querySelectorAll('input[name="color-type"]')
    
    let colorType = 'neutral'
    colorTypeInputs.forEach(input => {
      if (input.checked) {
        colorType = input.value
      }
    })
    
    const brightness = parseInt(brightnessInput.value)
    const temperature = getTemperatureKelvin(colorType)
    
    // Send command
    setZoneLighting(zone, temperature, brightness)
    
    // Close modal
    const modal = form.closest('.modal')
    if (modal) {
      modal.classList.remove('active')
    }
  })
  
  // Fridge lighting modal save (global modes)
  document.addEventListener('click', (e) => {
    const saveBtn = e.target.closest('.fridge-lighting-modal-save')
    if (!saveBtn) return
    
    const form = saveBtn.closest('.form')
    
    // Get checkbox values
    const savingMode = form.querySelector('input[name="saving-mode"]')?.checked || false
    const nightMode = form.querySelector('input[name="night-mode"]')?.checked || false
    
    // Apply appropriate mode
    if (nightMode) {
      setLightingMode('night_mode')
    } else if (savingMode) {
      setLightingMode('energy_saving')
    } else {
      setLightingMode('normal')
    }
    
    // Close modal
    const modal = form.closest('.modal')
    if (modal) {
      modal.classList.remove('active')
    }
  })
  
  // Individual drawer lighting controls
  document.addEventListener('click', (e) => {
    const drawerBtn = e.target.closest('.drawer-lighting-btn')
    if (!drawerBtn) return
    
    const drawer = drawerBtn.dataset.drawer
    const brightness = parseInt(drawerBtn.dataset.brightness || 70)
    const temperature = parseInt(drawerBtn.dataset.temperature || COLOR_TEMPS.NEUTRAL)
    
    setDrawerLighting(drawer, temperature, brightness)
  })
}

/**
 * Initialize lighting control module
 */
export function initLightingControl() {
  // Connect to MQTT if not already connected
  connectMQTT({
    host: BROKER_URL,
    options: { clientId: 'lighting-control-client' }
  })
  
  // Setup UI handlers
  setupLightingHandlers()
  
  // Subscribe to lighting status updates
  subscribe('winefridge/+/status', (message) => {
    try {
      const data = JSON.parse(message)
      if (data.action === 'lighting_status') {
        console.log('[LIGHTING] Status update:', data)
        // Update UI with current lighting status if needed
        updateLightingUI(data)
      }
    } catch (error) {
      console.error('[LIGHTING] Error parsing status:', error)
    }
  })
  
  console.log('[LIGHTING] Control module initialized')
}

/**
 * Update UI with current lighting status
 */
function updateLightingUI(data) {
  // Update brightness sliders
  const brightnessInputs = document.querySelectorAll('input[name="brightness"]')
  brightnessInputs.forEach(input => {
    if (input.dataset.zone === data.zone) {
      input.value = data.brightness
    }
  })
  
  // Update color temperature radio buttons
  const colorInputs = document.querySelectorAll('input[name="color-type"]')
  colorInputs.forEach(input => {
    if (input.dataset.zone === data.zone) {
      // Determine which color type based on temperature
      let colorType = 'neutral'
      if (Math.abs(data.temperature - COLOR_TEMPS.WARM) < 100) {
        colorType = 'warm'
      } else if (Math.abs(data.temperature - COLOR_TEMPS.COOL) < 100) {
        colorType = 'cool'
      }
      
      if (input.value === colorType) {
        input.checked = true
      }
    }
  })
}

// Auto-initialize when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initLightingControl)
} else {
  initLightingControl()
}
