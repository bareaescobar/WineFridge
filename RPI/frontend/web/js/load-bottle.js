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
    
    // FIXED: 'action' now inside 'data'
    const data = {
      action: 'start_load',
      barcode: scannedBottle.barcode,
      name: scannedBottle.name,
    }
    const payload = {
      timestamp: new Date().toISOString(),
      source: 'web',
      data: data
    }
    
    const message = JSON.stringify(payload)
    publish(TOPICS.WEB_TO_RPI_COMMAND, message)
  },
  'load-bottle-welcome-modal': () => {
    // If coming from info modal (user pressed back), cancel the load operation
    const fromInfoModal = loadBottleInfoModal?.classList.contains('active')
    if (fromInfoModal && scannedBottle) {
      console.log('[LOAD] User cancelled from info modal, sending cancel_load')
      const data = {
        action: 'cancel_load',
        barcode: scannedBottle.barcode
      }
      const payload = {
        timestamp: new Date().toISOString(),
        source: 'web',
        data: data
      }
      publish(TOPICS.WEB_TO_RPI_COMMAND, JSON.stringify(payload))
      scannedBottle = null  // Clear scanned bottle
    }

    // If coming from drawer modal (user pressed back during placement), cancel the load operation
    const fromDrawerModal = loadBottleDrawerModal?.classList.contains('active')
    if (fromDrawerModal && scannedBottle) {
      console.log('[LOAD] User cancelled from drawer modal, sending cancel_load')
      const data = {
        action: 'cancel_load',
        barcode: scannedBottle.barcode
      }
      const payload = {
        timestamp: new Date().toISOString(),
        source: 'web',
        data: data
      }
      publish(TOPICS.WEB_TO_RPI_COMMAND, JSON.stringify(payload))
      scannedBottle = null  // Clear scanned bottle
    }

    // If coming from error modal, send retry_placement to backend
    const fromErrorModal = loadBottleErrorModal?.classList.contains('active')
    if (fromErrorModal) {
      // FIXED: 'action' now inside 'data'
      const data = {
        action: 'retry_placement'
      }
      const payload = {
        timestamp: new Date().toISOString(),
        source: 'web',
        data: data
      }
      publish(TOPICS.WEB_TO_RPI_COMMAND, JSON.stringify(payload))
    }

    // If coming from success modal, send load_complete to change LED to white
    const fromSuccessModal = loadBottleSuccessModal?.classList.contains('active')
    if (fromSuccessModal) {
      // FIXED: 'action' now inside 'data'
      const data = {
        action: 'load_complete'
      }
      const payload = {
        timestamp: new Date().toISOString(),
        source: 'web',
        data: data
      }
      publish(TOPICS.WEB_TO_RPI_COMMAND, JSON.stringify(payload))
    }

    loadBottleInfoModal.classList.remove('active')
    loadBottleDrawerModal.classList.remove('active')
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
      // FIXED: 'action' now inside 'data'
      const returnData = {
        action: 'start_return',
        barcode: data.barcode,
        locations,
      }
      const payload = {
        timestamp: new Date().toISOString(),
        source: 'web',
        data: returnData
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
    
    // FIXED: 'action' now inside 'data'
    const data = {
      action: 'load_complete'
    }
    const payload = {
      timestamp: new Date().toISOString(),
      source: 'web',
      data: data
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