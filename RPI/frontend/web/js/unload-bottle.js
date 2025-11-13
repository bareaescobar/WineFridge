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
const unloadErrorModal = document.getElementById('unload-error-modal')
const unloadErrorTitle = unloadErrorModal?.querySelector('#unload-error-title')
const unloadErrorSubtitle = unloadErrorModal?.querySelector('#unload-error-subtitle')
const list = unloadBottleManuallyModal.querySelector('.products-list')
const recommendList = mealRecommendModal.querySelector('.recommend-group-list')
const bottleInfoContainer = unloadBottleInfoModal.querySelector('.block-bottle-info')

const port = 3000

let inventory = fetchSync(`http://localhost:${port}/inventory`)
let products = []

function refreshInventory() {
  inventory = fetchSync(`http://localhost:${port}/inventory`)

  const barcodes = Object.values(inventory.drawers).flatMap((drawer) =>
    Object.values(drawer.positions)
      .filter((pos) => pos.occupied && pos.barcode)
      .map((pos) => pos.barcode),
  )
  const barcodesSet = new Set(barcodes)

  products = Object.entries(wineCatalog.wines)
    .filter(([barcode]) => barcodesSet.has(barcode))
    .map(([barcode, product]) => ({
      ...product,
      barcode,
    }))

  updateSuggestionTemplate(products, list)
}

refreshInventory()

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
    // If coming from error modal, just go back to drawer modal (don't cancel operation)
    const fromErrorModal = unloadErrorModal?.classList.contains('active')
    if (fromErrorModal) {
      console.log('[UNLOAD] User acknowledged error, going back to drawer modal')
      unloadErrorModal.classList.remove('active')
      unloadBottleDrawerModal.classList.add('active')
      return
    }

    // If coming from drawer modal (user pressed back during unload), cancel the unload operation
    const fromDrawerModal = unloadBottleDrawerModal?.classList.contains('active')
    if (fromDrawerModal) {
      console.log('[UNLOAD] User cancelled from drawer modal, sending cancel_unload')
      const data = {
        action: 'cancel_unload'
      }
      const payload = {
        timestamp: new Date().toISOString(),
        source: 'web',
        data: data
      }
      publish(TOPICS.WEB_TO_RPI_COMMAND, JSON.stringify(payload))
    }

    unloadBottleSuggestModal.classList.contains('active') && unloadBottleSuggestModal.classList.remove('active')
    mealRecommendModal.classList.contains('active') && mealRecommendModal.classList.remove('active')
    unloadBottleInfoModal.classList.contains('active') && unloadBottleInfoModal.classList.remove('active')
    unloadBottleDrawerModal.classList.contains('active') && unloadBottleDrawerModal.classList.remove('active')
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
    
    // FIXED: 'action' now inside 'data'
    const data = {
      action: 'start_unload',
      barcode: pickedProduct.barcode,
      name: pickedProduct.name,
    }
    const payload = {
      timestamp: new Date().toISOString(),
      source: 'web',
      data: data
    }
    
    const message = JSON.stringify(payload)
    publish(TOPICS.WEB_TO_RPI_COMMAND, message)
  },
}

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

  // Handle wrong bottle removed during unload
  wrong_bottle_removed(data) {
    console.log('[UNLOAD] Wrong bottle removed:', data)

    if (unloadErrorModal) {
      // Update modal text with specific positions
      if (unloadErrorTitle) {
        unloadErrorTitle.innerHTML = `Wrong Bottle<br/>Removed`
      }
      if (unloadErrorSubtitle) {
        unloadErrorSubtitle.innerHTML = `You removed position <strong>${data.position}</strong>.<br/>Please remove position <strong>${data.expected_position}</strong> instead.<br/><br/>LEDs show: <span style="color: red">RED = wrong</span>, <span style="color: green">GREEN = correct</span>`
      }

      // Hide drawer modal and show error modal
      unloadBottleDrawerModal.classList.remove('active')
      unloadErrorModal.classList.add('active')
    } else {
      // Fallback to alert if modal doesn't exist
      alert(`⚠️ Wrong Bottle Removed\n\nYou removed position ${data.position}.\nPlease remove position ${data.expected_position} instead.`)
    }
  },

  // Handle wrong bottle placed back during unload
  wrong_bottle_replaced(data) {
    console.log('[UNLOAD] Wrong bottle placed back:', data)
    // Close error modal and show drawer modal again
    unloadErrorModal.classList.remove('active')
    unloadBottleDrawerModal.classList.add('active')
  },

  // Handle other unload errors (kept for compatibility)
  unload_error(data) {
    console.log('[UNLOAD] Error detected:', data)
    alert(`⚠️ Unload Error\n\n${data.error || 'Unknown error'}`)
  },
}

connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'unload-bottle-client' },
})
subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))
searchHandler('.search-wrapper')