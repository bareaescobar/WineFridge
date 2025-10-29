import {
  fetchSync,
  getBottleDetails,
  handleMQTTMessage,
  searchHandler,
  updateBottleInfoModalWithPosition,
  updateSuggestionTemplate,
} from './helpers/helpers'
import { connectMQTT, publish, subscribe } from './mqttClient'
import { TOPICS } from './constants/topics'
import { BROKER_URL } from './constants/mqtt-variables'
import wineCatalog from '../../../database/wine-catalog.json'

const unloadBottleSuggestModal = document.getElementById('unload-bottle-suggest-modal')
const unloadBottleManuallyModal = document.getElementById('unload-bottle-manually-modal')
const unloadBottleInfoModal = document.getElementById('unload-bottle-info-modal')
const unloadBottleDrawerModal = document.getElementById('take-bottle-drawer-modal')
const mealRecommendModal = document.getElementById('meal-recommend-modal')
const unloadBottleSuccessModal = document.getElementById('take-bottle-success-modal')
const list = unloadBottleManuallyModal.querySelector('.products-list')
const recommendList = mealRecommendModal.querySelector('.recommend-group-list')
const bottleInfoContainer = unloadBottleInfoModal.querySelector('.block-bottle-info')

const port = 3000

const inventory = fetchSync(`http://localhost:${port}/inventory`)

const barcodes = Object.values(inventory.drawers).flatMap((drawer) =>
  Object.values(drawer.positions)
    .filter((pos) => pos.occupied && pos.barcode)
    .map((pos) => pos.barcode),
)
const barcodesSet = new Set(barcodes);

const products = Object.entries(wineCatalog.wines)
  .filter(([barcode]) => barcodesSet.has(barcode))
  .map(([barcode, product]) => ({
    ...product,
    barcode,
  }))

const modalActions = {
  'unload-bottle-info-modal': (btn) => {
    const inventory = fetchSync(`http://localhost:${port}/inventory`)
    const btnText = btn.querySelector('.product-item-title').textContent
    const pickedProduct = products.find((product) => product.name === btnText)
    const pickedProductPositionDetails = getBottleDetails(pickedProduct.barcode, inventory)
    const confirmBtn = unloadBottleInfoModal.querySelector('[data-target="take-bottle-drawer-modal"]')
    confirmBtn.dataset.barcode = pickedProduct.barcode
    updateBottleInfoModalWithPosition(pickedProduct, pickedProductPositionDetails, bottleInfoContainer)
  },
  'unload-bottle-welcome-modal': () => {
    unloadBottleSuggestModal.classList.contains('active') && unloadBottleSuggestModal.classList.remove('active')
    mealRecommendModal.classList.contains('active') && mealRecommendModal.classList.remove('active')
    unloadBottleInfoModal.classList.contains('active') && unloadBottleInfoModal.classList.remove('active')
    unloadBottleSuccessModal.classList.contains('active') && unloadBottleSuccessModal.classList.remove('active')
    unloadBottleManuallyModal.classList.contains('active') && unloadBottleManuallyModal.classList.remove('active')
  },

  'meal-recommend-modal': (btn) => {
    const btnText = btn.querySelector('.suggest-item-title').textContent
    const modalTitleSelectedTextElem = mealRecommendModal.querySelector('.modal-title span')
    const modalSubitleSelectedTextElem = mealRecommendModal.querySelector('.modal-subtitle span')

    modalTitleSelectedTextElem.textContent = btnText
    modalSubitleSelectedTextElem.textContent = btnText
    const filteredProductsBySuggestion = products.filter((product) => {
      return product.meal_type.includes(btnText) || product.atmosphere.includes(btnText)
    })
    updateSuggestionTemplate(filteredProductsBySuggestion, recommendList)
  },
  'take-bottle-drawer-modal': (btn) => {
    const pickedProduct = products.find((product) => product.barcode === btn.dataset.barcode)
    const payload = {
      action: 'start_unload',
      source: 'web',
      timestamp: new Date().toISOString(),
      data: {
        barcode: pickedProduct.barcode,
        name: pickedProduct.name,
      },
    }
    const message = JSON.stringify(payload)
    publish(TOPICS.WEB_TO_RPI_COMMAND, message)
  },
}

updateSuggestionTemplate(products, list)

document.body.addEventListener('click', (e) => {
  const btn = e.target.closest('[data-target]')
  if (!btn) return

  const targetId = btn.dataset.target
  const target = document.getElementById(targetId)
  if (!target) return

  target.classList.add('active')
  modalActions[targetId]?.(btn)
})

const mqttActions = {
  bottle_unloaded(data) {
    console.log('[UNLOAD] Bottle unloaded successfully:', data)
    unloadBottleDrawerModal.classList.remove('active')
    unloadBottleSuccessModal.classList.add('active')
  },

  // Handle wrong position error during unload
  unload_error(data) {
    console.log('[UNLOAD] Error detected:', data)

    if (data.error === 'wrong_bottle_removed') {
      // Show simple alert (can be replaced with a nice modal later)
      alert(`⚠️ Wrong Bottle!\n\nYou removed bottle from position ${data.wrong_position}\nPlease remove from position ${data.correct_position}\n\nLEDs show: RED = wrong, GREEN = correct`)
    } else {
      // Generic error
      alert(`⚠️ Unload Error\n\n${data.error || 'Unknown error'}`)
    }
  },
}

connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'unload-bottle-client' },
})
subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))
searchHandler('.search-wrapper')
