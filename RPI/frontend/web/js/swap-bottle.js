import { connectMQTT, subscribe, publish } from './mqttClient'
import { TOPICS } from './constants/topics'
import { BROKER_URL } from './constants/mqtt-variables'
import { fetchSync, handleMQTTMessage, updateProductView } from './helpers/helpers'
import wineCatalog from '../../../database/wine-catalog.json'

const port = 3000

const swapBottlesModal = document.getElementById('swap-bottles-modal')
const swapBottlesSuccessModal = document.getElementById('swap-bottles-success-modal')
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
  
  // Notify backend to start swap mode
  const payload = {
    action: 'start_swap',
    source: 'web',
    timestamp: new Date().toISOString()
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
  }
}

connectMQTT({ 
  host: BROKER_URL, 
  options: { clientId: 'swap-bottles-client' } 
})

subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))

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
