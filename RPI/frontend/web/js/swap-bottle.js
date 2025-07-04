import { connectMQTT, subscribe, publish } from './mqttClient'
import { TOPICS } from './constants/topics'
import { BROKER_URL } from './constants/mqtt-variables'
import { animateSwapPlaceholders, fetchSync, handleMQTTMessage, updateProductView } from './helpers/helpers'
import wineCatalog from '../../../database/wine-catalog.json'

const port = 3000

const swapBottlesModal = document.getElementById('swap-bottles-modal')
const swapBottlesSuccessModal = document.getElementById('swap-bottles-success-modal')
const swapGroup = document.querySelector('.swap-group')
const bottleBtns = swapGroup.querySelectorAll('.bottle-placeholder')
const [firstBtn, secondBtn] = bottleBtns
const [firstText, secondText] = [firstBtn, secondBtn].map((btn) => btn.querySelector('.placeholder-text'))
const [firstProduct, secondProduct] = [firstBtn, secondBtn].map((btn) => btn.querySelector('.product-item'))

const removed = []
const placed = []

function resetScene() {
  swapBottlesModal.classList.remove('active')
  firstText.classList.add('active')
  secondText.classList.add('active')
  firstProduct.classList.remove('active')
  secondProduct.classList.remove('active')
}

function handleSwapResponse(success) {
  if (success) {
    swapBottlesSuccessModal.classList.add('active')
    resetScene()
  } else {
    swapBottlesSuccessModal.classList.remove('active')
  }
  const divider = swapGroup.querySelector('.divider')
  swapGroup.insertBefore(bottleBtns[0], divider)
  swapGroup.insertBefore(bottleBtns[1], divider.nextSibling)
  removed.length = 0
  placed.length = 0
}

const mqttActions = {
  bottle_event(data) {
    const { event, drawer, position } = data
    if (event === 'removed') {
      removed.push({ position, drawer })
      const inventory = fetchSync(`http://localhost:${port}/inventory`)
      const dr = inventory.drawers[drawer]
      const wine = wineCatalog.wines[dr.positions[position].barcode]
      if (removed.length === 1) updateProductView(firstProduct, firstText, wine)
      if (removed.length === 2) updateProductView(secondProduct, secondText, wine)
      if (removed.length > 2) removed.length = placed.length = 0
    }

    if (event === 'placed') {
      placed.push({ position, drawer })
      if (placed.length === 1) animateSwapPlaceholders()
    }

    if (removed.length === 2 && placed.length === 2) {
      const removedPositions = removed.map((item) => `${item.drawer}:${item.position}`).sort()
      const placedPositions = placed.map((item) => `${item.drawer}:${item.position}`).sort()
      if (JSON.stringify(removedPositions) === JSON.stringify(placedPositions)) {
        fetch(`http://localhost:${port}/swap-bottles`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ from: removed[0], to: removed[1] }),
        })
          .then((res) => res.json())
          .then((data) => handleSwapResponse(data.success))
          .catch((err) => {
            console.error('[SWAP] Error during swap:', err)
            swapBottlesSuccessModal.classList.remove('active')
          })
      } else {
        console.warn('[SWAP] Bottles not placed back into original positions — swap invalid')
      }
    }
  },
}

connectMQTT({ host: BROKER_URL, options: { clientId: 'swap-bottles-client' } })
subscribe(TOPICS.RPI_TO_WEB_EVENT, (rawMessage) => handleMQTTMessage(rawMessage, mqttActions))
// publish(
//   TOPICS.WEB_TO_RPI_COMMAND,
//   JSON.stringify({ action: 'start_swap', source: 'ui', timestamp: new Date().toISOString() }),
// )

document.addEventListener('click', (event) => {
  const btn = event.target.closest('[data-target]')
  if (!btn) return
  const target = btn.dataset.target
  if (target === 'swap-bottles-modal') {
    swapBottlesModal.classList.add('active')
    swapBottlesSuccessModal.classList.remove('active')
  }
})
