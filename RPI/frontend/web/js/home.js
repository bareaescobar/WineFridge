import {
  tabSelectorsHandler,
  wheelSelectorHandler,
  linearProgressHandler,
  codeModalsGroupHandler,
  controlsDropdownTogglersHandler,
  handleMQTTMessage,
  getZoneWinesAmount,
  getOccupiedPositions,
  getZoneFormData,
  fetchSync,
} from './helpers/helpers'

import { connectMQTT, publish, subscribe } from './mqttClient'
import { TOPICS } from './constants/topics'
import { BROKER_URL } from './constants/mqtt-variables'

const zonesConfig = [
  { drawers: ['drawer_1', 'drawer_2', 'drawer_3'], key: 'upper' },
  { drawers: ['drawer_4', 'drawer_5', 'drawer_6'], key: 'middle' },
  { drawers: ['drawer_7', 'drawer_8', 'drawer_9'], key: 'lower' },
]

const fridgePincodeOldModal = document.getElementById('fridge-pincode-old-modal')
const fridgePincodeNewModal = document.getElementById('fridge-pincode-new-modal')
const fridgePincodeConfirmModal = document.getElementById('fridge-pincode-confirm-modal')
const fridgePincodeSuccessModal = document.getElementById('fridge-pincode-success-modal')
const fridgePincodeModalsArray = [
  fridgePincodeOldModal,
  fridgePincodeNewModal,
  fridgePincodeConfirmModal,
  fridgePincodeSuccessModal,
]
const zoneParamsModal = document.getElementById('zone-params-modal')
const fridgeCapacity = document.querySelector('.fridge-capacity')
const fridgeCapacityStatus = fridgeCapacity.querySelector('.capacity-status span')
const fridgeCapacityProgress = fridgeCapacity.querySelector('.capacity-progress span')
const fridgeCapacityLeft = fridgeCapacity.querySelector('.capacity-left span')
const zonesList = Array.from(document.querySelector('.zones-list').children)
const scanCircle = document.querySelector('.processing')

function renderHomepage() {
  const inventory = fetchSync('http://localhost:5000/inventory')
  const fullFridgeAmount = Object.values(inventory.drawers).reduce(
    (acc, { positions }) => acc + Object.keys(positions).length,
    0,
  )
  const totalOccupied = zonesConfig.reduce((sum, zone) => sum + getZoneWinesAmount(zone.drawers), 0)
  const totalAvailable = fullFridgeAmount - totalOccupied
  const progressPercent = Math.round((totalOccupied * 100) / fullFridgeAmount)

  fridgeCapacityStatus.textContent = totalOccupied
  fridgeCapacityLeft.textContent = totalAvailable
  fridgeCapacityProgress.style.width = `${progressPercent}%`

  zonesList.forEach((zone, i) => {
    const [modeEl, winesEl, temperatureEl, humidityEl] = zone.querySelectorAll('.params-group-value')
    const drawerAmountEls = zone.querySelectorAll('.drawer-item-amount')
    const drawerCapacityEls = zone.querySelectorAll('.drawer-item-capacity')

    const { drawers, key } = zonesConfig[i]
    const winesAmount = getZoneWinesAmount(drawers)

    const [{ temperature, humidity, mode }] = drawers.map((drawer) => {
      return inventory.drawers[drawer]
    }) //temporary get first drawer data
    modeEl.textContent = mode.charAt(0).toUpperCase() + mode.slice(1)
    winesEl.textContent = winesAmount
    temperatureEl.textContent = `${temperature}°C`
    humidityEl.textContent = `${humidity}%`
    drawerAmountEls.forEach((el, idx) => {
      el.textContent = `${getOccupiedPositions(drawers[idx])} Wines`
    })
    drawerCapacityEls.forEach((el, idx) => {
      const occupied = getOccupiedPositions(drawers[idx])
      const span = el.querySelectorAll('span')[occupied]
      if (span) {
        el.insertBefore(document.createElement('br'), span)
      }
    })
  })
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
          fetch('http://localhost:5000/update-inventory', {
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
