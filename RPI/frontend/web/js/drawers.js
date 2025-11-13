import { favoriteHandler, fetchSync, handleMQTTMessage } from './helpers/helpers'
import wineCatalog from '../../../database/wine-catalog.json'
import {
  renderAllBottles,
  renderBottleInfoSection,
  renderDrawerTitle,
  renderWineInfo,
  renderZoneLabel,
} from './helpers/templates'
import { TOPICS } from './constants/topics'
import { BROKER_URL } from './constants/mqtt-variables'
import { connectMQTT, publish, subscribe } from './mqttClient'

const port = 3000
const drawerId = new URLSearchParams(location.search).get('drawer')

const takeWineModal = document.getElementById('take-bottle-drawer-modal')
const takeWineSuccessModal = document.getElementById('take-bottle-success-modal')
const unloadErrorModal = document.getElementById('unload-error-modal')
const bottleInfoModal = document.getElementById('bottle-info-modal')

const drawerElem = document.querySelector('.drawer')
const drawerTitleElem = document.querySelector('.drawer-title')
const zoneLabelElem = document.querySelector('.drawer-zone')
const bottlesContainer = document.querySelector('.drawer-bottles')
const wineInfoElem = document.querySelector('.bottle-head-info')
const bannerElem = document.querySelector('.bottle-head-banner img')

let selectedBottle = null

const inventory = fetchSync(`http://localhost:${port}/inventory`)
const { mode, zone, positions } = inventory.drawers[drawerId]

function updateActiveWineInfo() {
  const activeBottle = document.querySelector('.drawer-bottles .bottle.selected')
  const activeBarcode = activeBottle?.dataset.barcode
  // Store both barcode and drawer info in selectedBottle
  selectedBottle = {
    ...wineCatalog.wines[activeBarcode],
    barcode: activeBarcode,
    drawer: drawerId  // Add drawer ID from the page context
  }
  const buttons = wineInfoElem.querySelector('.bottle-btns')

  wineInfoElem.innerHTML = ''
  if (buttons) wineInfoElem.appendChild(buttons)
  wineInfoElem.insertAdjacentHTML('afterbegin', renderWineInfo(selectedBottle))
  bannerElem.src = selectedBottle.image_url || ''
}

function setupBottleClickHandler() {
  document.addEventListener('click', (event) => {
    const clickedBottle = event.target.closest('.drawer-bottles .bottle')
    if (!clickedBottle) return

    document.querySelectorAll('.drawer-bottles .bottle').forEach((bottle) => bottle.classList.remove('selected'))
    clickedBottle.classList.add('selected')
    updateActiveWineInfo()
  })
}

function setupModalButtonsHandler() {
  document.addEventListener('click', (event) => {
    const btn = event.target.closest('[data-target]')
    if (!btn) return

    const target = document.getElementById(btn.dataset.target)
    if (target) target.classList.add('active')

    if (btn.dataset.target === 'take-bottle-drawer-modal') {
      // Only send start_unload if coming from drawer-modal, not from error modal
      const fromErrorModal = btn.closest('#unload-error-modal')

      if (!fromErrorModal) {
        // FIXED: 'action' now inside 'data'
        const data = {
          action: 'start_unload',
          barcode: selectedBottle.barcode,
          name: selectedBottle.name,
          drawer: selectedBottle.drawer  // Include drawer ID to unload from correct drawer
        }
        const payload = {
          timestamp: new Date().toISOString(),
          source: 'web',
          data: data
        }

        const message = JSON.stringify(payload)
        publish(TOPICS.WEB_TO_RPI_COMMAND, message)
      }

      btn.closest('.modal')?.classList.remove('active')
    }

    if (btn.dataset.target === 'drawer-modal') {
      takeWineModal.classList.remove('active')
      takeWineSuccessModal.classList.remove('active')
      unloadErrorModal.classList.remove('active')
    }
    if (btn.dataset.target === 'bottle-info-modal') {
      renderBottleInfoSection(bottleInfoModal, selectedBottle, drawerId, zone, positions, mode, true)
    }
  })
}

function init() {
  drawerElem.classList.add(mode)
  drawerTitleElem.innerHTML = renderDrawerTitle(drawerId)
  zoneLabelElem.innerHTML = renderZoneLabel(zone, mode)
  bottlesContainer.innerHTML = renderAllBottles(positions)
  updateActiveWineInfo()
  setupBottleClickHandler()
  setupModalButtonsHandler()
  favoriteHandler('.favorite-btn')
}

const mqttActions = {
  expect_removal() {
    // Modal is already showing, just waiting for bottle removal
    console.log('[DRAWERS] Waiting for bottle removal...')
  },

  bottle_unloaded() {
    takeWineModal.classList.remove('active')
    takeWineSuccessModal.classList.add('active')
  },

  wrong_bottle_removed(data) {
    console.log('[DRAWERS] Wrong bottle removed:', data)
    // Hide drawer modal and show error modal
    takeWineModal.classList.remove('active')
    unloadErrorModal.classList.add('active')
  },

  wrong_bottle_replaced(data) {
    console.log('[DRAWERS] Wrong bottle placed back:', data)
    // Close error modal and show drawer modal again
    unloadErrorModal.classList.remove('active')
    takeWineModal.classList.add('active')
  },

  unload_timeout(data) {
    console.log('[DRAWERS] Unload timeout - returning to home')
    // Close all modals
    takeWineModal.classList.remove('active')
    takeWineSuccessModal.classList.remove('active')
    unloadErrorModal.classList.remove('active')

    // Redirect to home after a brief delay
    setTimeout(() => {
      window.location.href = '/'
    }, 100)
  },
}

connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'drawer-info-client' },
})
subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))

init()