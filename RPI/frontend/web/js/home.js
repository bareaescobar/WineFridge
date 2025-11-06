import {
  tabSelectorsHandler,
  wheelSelectorHandler,
  linearProgressHandler,
  codeModalsGroupHandler,
  controlsDropdownTogglersHandler,
  handleMQTTMessage,
  getZoneFormData,
  fetchSync,
  getZoneWinesAmount,
  getFridgeLighting,
  getZoneLightning,
} from './helpers/helpers'

import { connectMQTT, publish, subscribe } from './mqttClient'
import { TOPICS } from './constants/topics'
import { BROKER_URL } from './constants/mqtt-variables'
import { renderFridgeCapacity, renderZone } from './helpers/templates'

const zonesConfig = [
  { drawers: ['drawer_9', 'drawer_8', 'drawer_7'], key: 'upper' },
  { drawers: ['drawer_6', 'drawer_5', 'drawer_4'], key: 'middle' },
  { drawers: ['drawer_3', 'drawer_2', 'drawer_1'], key: 'lower' },
]

const port = 3000

const fridgePincodeOldModal = document.getElementById('fridge-pincode-old-modal')
const fridgePincodeNewModal = document.getElementById('fridge-pincode-new-modal')
const fridgePincodeConfirmModal = document.getElementById('fridge-pincode-confirm-modal')
const fridgePincodeSuccessModal = document.getElementById('fridge-pincode-success-modal')
const fridgeCapacity = document.querySelector('.fridge-capacity')
const zoneParamsModal = document.getElementById('zone-params-modal')
const scanCircle = document.querySelector('.processing')
const fridgePincodeModalsArray = [
  fridgePincodeOldModal,
  fridgePincodeNewModal,
  fridgePincodeConfirmModal,
  fridgePincodeSuccessModal,
]

function renderZonesHTML(zonesConfig, inventory) {
  return zonesConfig.map((zone) => renderZone(zone, inventory)).join('')
}

function renderHomepage() {
  const inventory = fetchSync(`http://localhost:${port}/inventory`)
  const current = zonesConfig.reduce((sum, zone) => sum + getZoneWinesAmount(zone.drawers, inventory), 0)
  const fridgeHTML = renderFridgeCapacity({ current })
  fridgeCapacity.innerHTML = fridgeHTML
  document.querySelector('.zones-list').innerHTML = ''
  document.querySelector('.zones-list').innerHTML = renderZonesHTML(zonesConfig, inventory)
}

const modalActions = {
  'fridge-pincode-old-modal': () => {
    codeModalsGroupHandler(fridgePincodeModalsArray)
  },
  'zone-params-modal': (btn) => {
    const zoneName = btn.querySelector('.text')?.textContent.trim()
    const modal = document.getElementById('zone-params-modal')
    const form = modal.querySelector('.form')
    const headTitle = modal.querySelector('.modal-head-title')
    form.dataset.type = zoneName
    headTitle.textContent = zoneName.charAt(0).toUpperCase() + zoneName.slice(1)
  },
  'zone-params-modal-save': (btn) => {
    const form = btn.closest('.form')
    const formData = getZoneFormData(form)
    zonesConfig.forEach(({ key }) => {
      if (form.dataset.type.toLowerCase().includes(key)) {
        const { temperature: target, humidity, mode } = formData
        const data = {
          action: 'update_setting',
          mode,
          target,
          humidity,
          zone: key,
        }
        const payload = {
          timestamp: new Date().toISOString(),
          source: 'web_client',
          target: 'rpi_server',
          message_type: 'command',
          data,
        }
        const message = JSON.stringify(payload)
        publish(TOPICS.WEB_TO_RPI_COMMAND, message)
        ////temporary fetch request to update inventory
        try {
          fetch(`http://localhost:${port}/update-inventory`, {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json',
            },
            body: JSON.stringify(data),
          })
            .then((res) => res.json())
            .then((response) => {
              scanCircle.classList.add('active')
            })
            .catch((error) => {
              console.error('Error updating inventory:', error)
            })
        } catch (error) {
          console.error('Error updating zone settings:', error)
        }
      }
    })
  },
  'zone-lighting-modal-save': (btn) => {
    const form = btn.closest('.form')
    const key = form.dataset.key
    const formData = getZoneLightning(form)

    // Build the core command data
    const commandData = {
      action: 'set_brightness', // <-- Top-level action
      zone: key,
      value: formData.brightness,
      color_temp: formData.colorType,
    }

    // Create a flat payload by spreading the command data
    const payload = {
      timestamp: new Date().toISOString(),
      source: 'web_client',
      target: 'rpi_server',
      message_type: 'command',
      ...commandData // <-- Use spread operator to merge all keys to top level
    }

    // The resulting JSON will now have 'action' at the top level
    const message = JSON.stringify(payload)
    publish(TOPICS.WEB_TO_RPI_COMMAND, message)
  },
  'fridge-lighting-modal-save': (btn) => {
    const form = btn.closest('.form')
    const formData = getFridgeLighting(form)
    const data = {
      action: 'set_lighting_mode',
      energy_saving: formData.savingMode,
      night_mode: formData.nightMode,
    }
    const payload = {
      timestamp: new Date().toISOString(),
      source: 'web_client',
      target: 'rpi_server',
      message_type: 'command',
      data,
    }
    const message = JSON.stringify(payload)
    publish(TOPICS.WEB_TO_RPI_COMMAND, message)
  },
  'zone-lighting-modal': (btn) => {
    const targetId = btn.dataset.target
    const targetKey = btn.dataset.key
    const target = document.getElementById(targetId)
    const name = btn.querySelector('.fridge-zone-name').textContent
    target.querySelector('.modal-head-title').textContent = `${name} Lighting`
    target.querySelector('.form').dataset.key = targetKey
  },
}

document.body.addEventListener('click', (e) => {
  const saveBtn = e.target.closest('.form .save-form')
  const btn = e.target.closest('[data-target]')
  if (btn) {
    const targetId = btn.dataset.target
    const target = document.getElementById(targetId)
    if (!target) return

    target.classList.add('active')
    modalActions[targetId]?.(btn)
  } else if (saveBtn) {
    const targetId = saveBtn.closest('.modal').id + '-save'
    modalActions[targetId]?.(saveBtn)
  }
})

const mqttActions = {
  inventory_updated(data) {
    renderHomepage()
  },

  settings_updated() {
    renderHomepage()
    scanCircle.classList.remove('active')
    zoneParamsModal.classList.remove('active')
  },
}

tabSelectorsHandler('.tabs-group')
wheelSelectorHandler('.wheel-progress-group')
linearProgressHandler('.linear-progress-group')
controlsDropdownTogglersHandler('.controls-dropdown-toggler-btn')
renderHomepage()

connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'load-bottle-client' },
})

subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))
