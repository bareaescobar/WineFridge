import { BROKER_URL } from './constants/mqtt-variables'
import { TOPICS } from './constants/topics'
import {
  imgInputsHandler,
  favoriteHandler,
  handleMQTTMessage,
  updateBottleInfoModal,
  fetchSync,
} from './helpers/helpers'
import { connectMQTT, publish, subscribe } from './mqttClient'
import wineCatalog from '../../../database/wine-catalog.json'

const loadBottleWelcomeModal = document.getElementById('load-bottle-welcome-modal')
const scanBottleErrorModal = document.getElementById('scan-bottle-error-modal')
const loadBottleInfoModal = document.getElementById('load-bottle-info-modal')
const loadBottleDrawerModal = document.getElementById('load-bottle-drawer-modal')
const loadBottleSuccessModal = document.getElementById('load-bottle-success-modal')
const loadBottleErrorModal = document.getElementById('load-bottle-error-modal')

const scanCircle = loadBottleWelcomeModal.querySelector('.processing')

const port = 3000
let scannedBottle = null

const modalActions = {
  'load-bottle-drawer-modal': () => {
    scanCircle.classList.add('active')
    loadBottleInfoModal.classList.remove('active')
    const payload = {
      action: 'start_load',
      source: 'web',
      timestamp: new Date().toISOString(),
      data: {
        barcode: scannedBottle.barcode,
        name: scannedBottle.name,
      },
    }
    const message = JSON.stringify(payload)
    publish(TOPICS.WEB_TO_RPI_COMMAND, message)
  },
  'load-bottle-welcome-modal': () => {
    // If coming from error modal, send retry_placement to backend
    const fromErrorModal = loadBottleErrorModal?.classList.contains('active')
    if (fromErrorModal) {
      const payload = {
        action: 'retry_placement',
        source: 'web',
        timestamp: new Date().toISOString()
      }
      publish(TOPICS.WEB_TO_RPI_COMMAND, JSON.stringify(payload))
    }

    // If coming from success modal, send load_complete to change LED to white
    const fromSuccessModal = loadBottleSuccessModal?.classList.contains('active')
    if (fromSuccessModal) {
      const payload = {
        action: 'load_complete',
        source: 'web',
        timestamp: new Date().toISOString()
      }
      publish(TOPICS.WEB_TO_RPI_COMMAND, JSON.stringify(payload))
    }

    loadBottleInfoModal.classList.remove('active')
    loadBottleErrorModal.classList.remove('active')
    loadBottleSuccessModal.classList.remove('active')
    scanCircle.classList.remove('active')
  },
}

const mqttActions = {
  barcode_scanned(data) {
    const extracted = fetchSync(`http://localhost:${port}/extracted`)
    const locations = extracted[data.barcode]?.locations
    if (locations) {
      const payload = {
        action: 'start_return',
        source: 'web',
        timestamp: new Date().toISOString(),
        data: {
          barcode: data.barcode,
          locations,
        },
      }
      const message = JSON.stringify(payload)
      publish(TOPICS.WEB_TO_RPI_COMMAND, message)
      scanCircle.classList.add('active')
      return
    }

    const bottle = wineCatalog.wines[data?.barcode]
    if (bottle) {
      scannedBottle = { ...bottle, barcode: data.barcode }
      updateBottleInfoModal(scannedBottle, loadBottleInfoModal)
      loadBottleInfoModal.classList.add('active')
    } else {
      scanBottleErrorModal.classList.add('active')
    }
  },
  
  expect_bottle() {
    scanCircle.classList.remove('active')
    loadBottleDrawerModal.classList.add('active')
  },
  
  // mqtt_handler 27.10.2025 sends "placement_error" for wrong position
  placement_error(data) {
    console.log('[LOAD] Wrong position detected:', data)
    loadBottleDrawerModal.classList.remove('active')
    loadBottleErrorModal.classList.add('active')
  },
  
  bottle_placed(data) {
    if (data.success) {
      loadBottleDrawerModal.classList.remove('active')
      loadBottleErrorModal.classList.remove('active')
      loadBottleSuccessModal.classList.add('active')
      scannedBottle = null
    } else if (data.close_screen) {
      // Timeout occurred - close all load modals and return to welcome screen
      console.log('[LOAD] Timeout - closing all modals')
      loadBottleDrawerModal.classList.remove('active')
      loadBottleErrorModal.classList.remove('active')
      loadBottleInfoModal.classList.remove('active')
      scanCircle.classList.remove('active')
      scannedBottle = null
    }
  },
  
  wrong_bottle_removed(data) {
    console.log('[LOAD] Wrong bottle removed, showing drawer modal again')
    loadBottleErrorModal.classList.remove('active')
    loadBottleDrawerModal.classList.add('active')
  },
}

document.addEventListener('click', (event) => {
  const btn = event.target.closest('[data-target]')
  if (!btn) return

  const target = btn.dataset.target
  const modalId = btn.closest('.modal')?.id
  if (modalActions[target]) {
    modalActions[target](modalId)
  }
})

// Handle "Done" button - send load_complete and redirect to home
const loadDoneBtn = document.getElementById('load-done-btn')
if (loadDoneBtn) {
  loadDoneBtn.addEventListener('click', () => {
    console.log('[LOAD] Done clicked - completing load and going home')
    const payload = {
      action: 'load_complete',
      source: 'web',
      timestamp: new Date().toISOString()
    }
    publish(TOPICS.WEB_TO_RPI_COMMAND, JSON.stringify(payload))

    // Small delay to ensure message is sent before redirect
    setTimeout(() => {
      window.location.href = '/'
    }, 100)
  })
}

connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'load-bottle-client' },
})

subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))
imgInputsHandler('.img-file-wrapper')
favoriteHandler('.favorite-btn')
