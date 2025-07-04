import { connectMQTT, subscribe, publish } from './mqttClient'
import { BROKER_URL } from './constants/mqtt-variables'
import { TOPICS } from './constants/topics'
import { fetchSync, handleMQTTMessage } from './helpers/helpers'
import wineCatalog from '../../../database/wine-catalog.json'
import Swiper from '../../node_modules/swiper/swiper-bundle'

const port = 3000
const TIMER = 5

let timeLeft = TIMER
let interval = null
let takenProductsAmount = 0
let activeBottle = null
let takenBottles = fetchSync(`http://localhost:${port}/extracted`)

const zoneParamsModal = document.getElementById('zone-params-modal')
const fridgeInfoModal = document.getElementById('fridge-info-modal')

const unauthorizedUnloadModal = document.getElementById('unauthorized-unload-modal')
const unauthorizedUnloadSuccessModal = document.getElementById('unauthorized-unload-success-modal')
const unauthorizedReturnedSuccessModal = document.getElementById('unauthorized-returned-success-modal')
const titleElem = unauthorizedUnloadModal.querySelector('.modal-title')
const titleAmountElem = titleElem.querySelector('strong')
const titlePreffixElem = titleElem.querySelector('span')
const counterElem = unauthorizedUnloadModal.querySelector('.counter')
const counterNumbersElem = counterElem.querySelector('.counter-numbers')
const swiperContainer = unauthorizedUnloadModal.querySelector('.swiper-container')

const swiper = new Swiper(swiperContainer, {
  loop: false,
  grabCursor: true,
  slidesPerView: 3,
  spaceBetween: 5,
  centeredSlides: true,
  initialSlide: 0,
})

function initializeModal() {
  if (!unauthorizedUnloadModal.classList.contains('active')) {
    unauthorizedUnloadModal.classList.add('active')
    zoneParamsModal.classList.remove('active')
    fridgeInfoModal.classList.remove('active')

    swiper.removeAllSlides()
    takenProductsAmount = 0
    titleElem.classList.remove('active')

    takenBottles = fetchSync(`http://localhost:${port}/extracted`)
    Object.entries(takenBottles).forEach(([barcode]) => {
      const wine = wineCatalog.wines[barcode]
      if (!wine) return
      appendTakenProductUI(wine, barcode)
    })
  }
}

function appendTakenProduct({ img, name, volume }, barcode) {
  return `
    <div class="product-item swiper-slide" data-barcode="${barcode}">
      <div class="product-item-photo">
        <img src="${img}" alt="" />
      </div>
      <div class="product-item-info">
        <div class="product-item-title">${name}</div>
        <div class="product-item-volume">${volume}</div>
      </div>
    </div>`
}

function appendTakenProductUI(wine, barcode) {
  takenProductsAmount++
  titleElem.classList.add('active')
  titleAmountElem.textContent = takenProductsAmount
  titlePreffixElem.textContent = takenProductsAmount === 1 ? 'Bottle' : 'Bottles'
  const slideHTML = appendTakenProduct(wine, barcode)
  swiper.appendSlide(slideHTML)
  swiper.slideTo(swiper.slides.length - 1, 500)
}

async function extractBottle(data) {
  const response = await fetch(`http://localhost:${port}/add-extracted-bottle`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data),
  })
  if (!response.ok) {
    throw new Error(`HTTP error! status: ${response.status}`)
  }
  return await response.json()
}

async function removeBottle(data) {
  try {
    const response = await fetch(`http://localhost:${port}/remove-bottle`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data),
    })
    const result = await response.json()
    if (response.ok) {
      console.log('✅ Bottle removed:', result)
    } else {
      console.error('❌ Failed to remove:', result.error)
    }
  } catch (error) {
    console.error('❌ Fetch error:', error)
  }
}

function startCounter(bottle) {
  takenBottles = fetchSync(`http://localhost:${port}/extracted`)
  unauthorizedUnloadModal.classList.add('active')
  zoneParamsModal.classList.remove('active')
  fridgeInfoModal.classList.remove('active')
  timeLeft = TIMER
  counterElem.classList.add('active')
  counterNumbersElem.textContent = timeLeft
  if (interval) {
    clearInterval(interval)
  }
  interval = setInterval(() => {
    timeLeft--
    counterNumbersElem.textContent = timeLeft
    if (timeLeft === 0) {
      clearInterval(interval)
      interval = null
      removeBottle(bottle)
      showBottleSuccess()
    }
  }, 1000)
}

function showBottleSuccess() {
  unauthorizedUnloadSuccessModal.classList.add('active')
}

function showReturnSuccess() {
  unauthorizedReturnedSuccessModal.classList.add('active')
}

const mqttActions = {
  bottle_event: async ({ event, drawer, position }) => {
    unauthorizedUnloadSuccessModal.classList.remove('active')
    unauthorizedReturnedSuccessModal.classList.remove('active')
    const inventory = fetchSync(`http://localhost:${port}/inventory`)
    const barcode = inventory.drawers[drawer].positions[position].barcode
    const wine = wineCatalog.wines[barcode]
    if (!wine) return
    const bottle = { drawer, position, barcode }
    if (event === 'removed') {
      try {
        initializeModal()
        await extractBottle(bottle)
        if (interval) {
          await removeBottle(activeBottle)
        }
        activeBottle = bottle
        appendTakenProductUI(wine, barcode)
        startCounter(activeBottle)
      } catch (error) {
        console.error('Error extracting bottle:', error)
      }
    }
  },
}

connectMQTT({ host: BROKER_URL, options: { clientId: 'unauthorized-unload' } })
subscribe(TOPICS.RPI_TO_WEB_EVENT, (msg) => handleMQTTMessage(msg, mqttActions))

publish(
  TOPICS.WEB_TO_RPI_COMMAND,
  JSON.stringify({ action: 'start_swap', source: 'ui', timestamp: new Date().toISOString() }),
)
