import { BROKER_URL } from './constants/mqtt-variables'
import { TOPICS } from './constants/topics'
import { imgInputsHandler, favoriteHandler, handleMQTTMessage, updateBottleInfoModal } from './helpers/helpers'
import { connectMQTT, publish, subscribe } from './mqttClient'
import wineCatalog from '../../../database/wine-catalog.json'

const loadBottleWelcomeModal = document.getElementById('load-bottle-welcome-modal')
const scanBottleErrorModal = document.getElementById('scan-bottle-error-modal')
const loadBottleInfoModal = document.getElementById('load-bottle-info-modal')
const loadBottleDrawerModal = document.getElementById('load-bottle-drawer-modal')
const loadBottleSuccessModal = document.getElementById('load-bottle-success-modal')
const loadBottleErrorModal = document.getElementById('load-bottle-error-modal')
// const editBottleModal = document.getElementById('edit-bottle-info-modal')
// const newBottleInfoModal = document.getElementById('new-bottle-info-modal')

const scanCircle = loadBottleWelcomeModal.querySelector('.processing')
// const errorModalTitle = loadBottleErrorModal.querySelector('.modal-title')

let scannedBottle = null

const modalActions = {
  'load-bottle-drawer-modal': () => {
    scanCircle.classList.add('active')
    loadBottleInfoModal.classList.remove('active')
    const payload = {
      timestamp: new Date().toISOString(),
      source: 'web',
      data: {
        action: 'start_load',
        barcode: scannedBottle.barcode,
        name: scannedBottle.name,
      },
    }
    const message = JSON.stringify(payload)
    publish(TOPICS.WEB_TO_RPI_COMMAND, message)
  },
  'load-bottle-welcome-modal': () => {
    loadBottleInfoModal.classList.remove('active')
    scanCircle.classList.remove('active')
  },
}

const mqttActions = {
  barcode_scanned(data) {
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
  bottle_placed(data) {
    loadBottleDrawerModal.classList.remove('active')
    const resultModal = data.success ? loadBottleSuccessModal : loadBottleErrorModal
    resultModal.classList.add('active')
    scannedBottle = null
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

connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'load-bottle-client' },
})

subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))
imgInputsHandler('.img-file-wrapper')
favoriteHandler('.favorite-btn')
