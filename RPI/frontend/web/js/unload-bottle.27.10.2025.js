import {
  handleMQTTMessage,
  searchHandler,
  updateBottleInfoModalWithPosition,
  updateSuggestionTemplate,
  fetchSync
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
const products = Object.entries(wineCatalog.wines).map(([barcode, product]) => ({
  ...product,
  barcode,
}))

// Local function to get bottle details from inventory (replaces imported getBottleDetails)
function getBottleDetailsFromInventory(barcode) {
  try {
    const inventory = fetchSync(`http://localhost:${port}/inventory`)
    
    if (!inventory || !inventory.drawers) {
      console.error('[UNLOAD] Invalid inventory structure')
      return null
    }
    
    // Search through all drawers for this barcode
    for (const [drawerId, drawer] of Object.entries(inventory.drawers)) {
      if (!drawer.positions) continue
      
      for (const [position, posData] of Object.entries(drawer.positions)) {
        if (posData && posData.occupied && posData.barcode === barcode) {
          return {
            drawer: drawerId,
            position: parseInt(position),
            weight: posData.weight || 0,
            placed_date: posData.placed_date,
            aging_days: posData.aging_days || 0,
            name: posData.name,
            ...posData
          }
        }
      }
    }
    return null
  } catch (error) {
    console.error('[UNLOAD] Error fetching inventory:', error)
    return null
  }
}

const modalActions = {
  'unload-bottle-info-modal': (btn) => {
    const btnText = btn.querySelector('.product-item-title')?.textContent
    if (!btnText) {
      console.error('[UNLOAD] No product title found')
      return
    }
    
    const pickedProduct = products.find((product) => product.name === btnText)
    if (!pickedProduct) {
      console.error('[UNLOAD] Product not found:', btnText)
      return
    }
    
    const pickedProductPositionDetails = getBottleDetailsFromInventory(pickedProduct.barcode)
    
    if (!pickedProductPositionDetails) {
      console.warn('[UNLOAD] Wine not in inventory:', pickedProduct.name)
      alert(`${pickedProduct.name} is not currently in the fridge`)
      return
    }
    
    const confirmBtn = unloadBottleInfoModal.querySelector('[data-target="take-bottle-drawer-modal"]')
    if (confirmBtn) {
      confirmBtn.dataset.barcode = pickedProduct.barcode
      confirmBtn.dataset.drawer = pickedProductPositionDetails.drawer
      confirmBtn.dataset.position = pickedProductPositionDetails.position
    }
    
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
    const btnText = btn.querySelector('.suggest-item-title')?.textContent
    if (!btnText) return
    
    const modalTitleSelectedTextElem = mealRecommendModal.querySelector('.modal-title span')
    const modalSubtitleSelectedTextElem = mealRecommendModal.querySelector('.modal-subtitle span')

    if (modalTitleSelectedTextElem) modalTitleSelectedTextElem.textContent = btnText
    if (modalSubtitleSelectedTextElem) modalSubtitleSelectedTextElem.textContent = btnText
    
    const filteredProductsBySuggestion = products.filter((product) => {
      return product.meal_type?.includes(btnText) || product.atmosphere?.includes(btnText)
    })
    updateSuggestionTemplate(filteredProductsBySuggestion, recommendList)
  },
  
  'take-bottle-drawer-modal': (btn) => {
    const barcode = btn.dataset.barcode
    if (!barcode) {
      console.error('[UNLOAD] No barcode found on button')
      return
    }
    
    const pickedProduct = products.find((product) => product.barcode === barcode)
    if (!pickedProduct) {
      console.error('[UNLOAD] Product not found for barcode:', barcode)
      return
    }
    
    // Send unload command (structure fixed - data was nested incorrectly)
    const payload = {
      action: 'start_unload',
      source: 'web',
      timestamp: new Date().toISOString(),
      data: {
        barcode: pickedProduct.barcode,
        name: pickedProduct.name
      }
    }
    const message = JSON.stringify(payload)
    console.log('[UNLOAD] Sending command:', payload)
    publish(TOPICS.WEB_TO_RPI_COMMAND, message)
  }
}

// Initialize suggestion list
updateSuggestionTemplate(products, list)

// Setup click handlers
document.body.addEventListener('click', (e) => {
  const btn = e.target.closest('[data-target]')
  if (!btn) return

  const targetId = btn.dataset.target
  const target = document.getElementById(targetId)
  if (!target) return

  target.classList.add('active')
  if (modalActions[targetId]) {
    modalActions[targetId](btn)
  }
})

// MQTT action handlers
const mqttActions = {
  expect_removal(data) {
    console.log('[UNLOAD] Expect removal:', data)
    // Show drawer modal when position is highlighted
    unloadBottleDrawerModal.classList.add('active')
  },
  
  bottle_unloaded(data) {
    console.log('[UNLOAD] Bottle unloaded:', data)
    unloadBottleDrawerModal.classList.remove('active')
    unloadBottleInfoModal.classList.remove('active')
    if (data.success) {
      unloadBottleSuccessModal.classList.add('active')
    }
  },
  
  unload_error(data) {
    console.error('[UNLOAD] Error:', data.error)
    alert(data.error || 'Error removing bottle')
  }
}

// Connect MQTT
connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'unload-bottle-client' },
})

// Subscribe to events
subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))

// Initialize search
searchHandler('.search-wrapper')
