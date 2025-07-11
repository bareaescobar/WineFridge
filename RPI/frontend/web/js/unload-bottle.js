import {
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

const products = Object.entries(wineCatalog.wines).map(([barcode, product]) => ({
  ...product,
  barcode,
}))

const modalActions = {
  'unload-bottle-info-modal': (btn) => {
    const btnText = btn.querySelector('.product-item-title').textContent
    const pickedProduct = products.find((product) => product.name === btnText)
    const pickedProductPositionDetails = getBottleDetails(pickedProduct.barcode)
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
      timestamp: new Date().toISOString(),
      source: 'web',
      data: {
        action: 'start_unload',
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
  bottle_unloaded() {
    unloadBottleDrawerModal.classList.remove('active')
    unloadBottleSuccessModal.classList.add('active')
  },
}

connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'unload-bottle-client' },
})
subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))
searchHandler('.search-wrapper')
