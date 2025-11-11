import { connectMQTT, subscribe, publish } from './mqttClient'
import { TOPICS } from './constants/topics'
import { BROKER_URL } from './constants/mqtt-variables'
import { fetchSync, handleMQTTMessage, updateProductView } from './helpers/helpers'
import wineCatalog from '../../../database/wine-catalog.json'

const port = 3000

const swapBottlesModal = document.getElementById('swap-bottles-modal')
const swapBottlesSuccessModal = document.getElementById('swap-bottles-success-modal')
const swapErrorModal = document.getElementById('swap-error-modal')
const swapErrorTitle = swapErrorModal?.querySelector('#swap-error-title')
const swapErrorSubtitle = swapErrorModal?.querySelector('#swap-error-subtitle')
const swapGroup = document.querySelector('.swap-group')
const bottleBtns = swapGroup.querySelectorAll('.bottle-placeholder')
const [firstBtn, secondBtn] = bottleBtns
const [firstText, secondText] = [firstBtn, secondBtn].map((btn) => btn.querySelector('.placeholder-text'))
const [firstProduct, secondProduct] = [firstBtn, secondBtn].map((btn) => btn.querySelector('.product-item'))

let bottlesRemoved = 0

function resetScene() {
  swapBottlesModal.classList.remove('active')
  firstText.classList.add('active')
  secondText.classList.add('active')
  firstProduct.classList.remove('active')
  secondProduct.classList.remove('active')
  bottlesRemoved = 0
}

function startSwapMode() {
  console.log('[SWAP] Starting swap mode')
  bottlesRemoved = 0
  
  // FIXED: 'action' now inside 'data'
  const data = {
    action: 'start_swap'
  }
  const payload = {
    timestamp: new Date().toISOString(),
    source: 'web',
    data: data
  }
  
  publish(TOPICS.WEB_TO_RPI_COMMAND, JSON.stringify(payload))
}

const mqttActions = {
  bottle_event(data) {
    const { event, drawer, position } = data
    console.log('[SWAP] Bottle event:', event, drawer, position)
    
    if (event === 'removed') {
      bottlesRemoved++
      console.log('[SWAP] Bottles removed:', bottlesRemoved)
      
      // Get wine info from inventory before it was removed
      const inventory = fetchSync(`http://localhost:${port}/inventory`)
      if (inventory && inventory.drawers && inventory.drawers[drawer]) {
        const posData = inventory.drawers[drawer].positions[position]
        
        if (posData && posData.barcode) {
          const wine = wineCatalog.wines[posData.barcode]
          if (wine) {
            if (bottlesRemoved === 1) {
              updateProductView(firstProduct, firstText, wine)
            } else if (bottlesRemoved === 2) {
              updateProductView(secondProduct, secondText, wine)
            }
          }
        }
      }
    }
  },
  
  // mqtt_handler 27.10.2025 sends this when swap completes
  swap_completed(data) {
    console.log('[SWAP] Swap completed:', data)
    if (data.success) {
      swapBottlesSuccessModal.classList.add('active')
      resetScene()
    } else {
      alert('Swap failed. Please try again.')
      resetScene()
    }
  },

  // Handle wrong position error during swap
  swap_error(data) {
    console.log('[SWAP] Error detected:', data)

    if (data.error === 'wrong_swap_position' && swapErrorModal) {
      const expectedPos = data.expected_positions ? data.expected_positions.join(' or ') : 'correct position'

      // Update modal text with specific positions
      if (swapErrorTitle) {
        swapErrorTitle.innerHTML = `Wrong Position<br/>During Swap`
      }
      if (swapErrorSubtitle) {
        swapErrorSubtitle.innerHTML = `You placed bottle at position <strong>${data.wrong_position}</strong>.<br/>Please place at position <strong>${expectedPos}</strong> instead.<br/><br/>LEDs show: <span style="color: red">RED = wrong</span>, <span style="color: green">GREEN = correct</span>`
      }

      // Show error modal
      swapErrorModal.classList.add('active')
    } else {
      // Fallback to alert for other errors
      alert(`⚠️ Swap Error\n\n${data.error || 'Unknown error'}`)
    }
  }
}

connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'swap-bottles-client' }
})

subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))

// Start swap mode automatically when page loads
startSwapMode()

document.addEventListener('click', (event) => {
  const btn = event.target.closest('[data-target]')
  if (!btn) return

  const target = btn.dataset.target
  if (target === 'swap-bottles-modal') {
    swapBottlesModal.classList.add('active')
    swapBottlesSuccessModal.classList.remove('active')
    startSwapMode()
  }
})