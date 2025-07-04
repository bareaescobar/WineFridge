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
  selectedBottle = { ...wineCatalog.wines[activeBarcode], barcode: activeBarcode }
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
      const payload = {
        timestamp: new Date().toISOString(),
        source: 'web',
        data: {
          action: 'start_unload',
          barcode: selectedBottle.barcode,
          name: selectedBottle.name,
        },
      }
      const message = JSON.stringify(payload)
      publish(TOPICS.WEB_TO_RPI_COMMAND, message)

      btn.closest('.modal')?.classList.remove('active')
    }

    if (btn.dataset.target === 'drawer-modal') {
      takeWineModal.classList.remove('active')
      takeWineSuccessModal.classList.remove('active')
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
  bottle_unloaded() {
    takeWineModal.classList.remove('active')
    takeWineSuccessModal.classList.add('active')
  },
}

connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'drawer-info-client' },
})
subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))

init()
