const unauthorizedUnloadModal = document.getElementById('unauthorized-unload-modal')
const unauthorizedUnloadSuccessModal = document.getElementById('unauthorized-unload-success-modal')

let timeLeft = 59
// let timeLeft = 60;
let interval = null
let takenProductsAmount = 0

const titleElem = unauthorizedUnloadModal.querySelector('.modal-title')
const titleAmountElem = titleElem.querySelector('.modal-title strong')
const titlePreffixElem = titleElem.querySelector('.modal-title span')
const productsList = unauthorizedUnloadModal.querySelector('.taken-products-inner')
const counterElem = unauthorizedUnloadModal.querySelector('.counter')
const counterNumbersElem = counterElem.querySelector('.counter-numbers')

// dummies
const firstProduct = {
  imgUrl: 'img/wine/Red/Bodegas Aragonesas-Solo-Syrah-Tirio.png',
  title: 'Amberwood Merlot',
  rating: '4.7',
  volume: '400ml',
}

const secondProduct = {
  imgUrl: 'img/wine/Red/Finca-del-Mar-Cabernet-Sauvignon-Crianza.png',
  title: 'Velvet Noir Reserve',
  rating: '4.0',
  volume: '400ml',
}

const thirdProduct = {
  imgUrl: 'img/wine/Red/Hoya-De-Cadenas-Cabarnet-Suavignon-Crianza.png',
  title: 'Golden Harvest',
  rating: '3.9',
  volume: '400ml',
}

const fourthProduct = {
  imgUrl: 'img/wine/Red/Laudium-Monastrell-Roble.png',
  title: 'Midnight Syrah',
  rating: '4.1',
  volume: '400ml',
}

function appendTakenProduct(productParams) {
  const { imgUrl, title, rating, volume } = productParams

  return `
        <div class="product-item">
            <div class="product-item-photo">
                <img src="${imgUrl}" alt="" />
            </div>
            <div class="product-item-info">
                <div class="product-item-title">${title}</div>
                <div class="product-item-rating">${rating}</div>
                <div class="product-item-volume">${volume}</div>
            </div>
        </div>
    `
}

function takeBottleEvent(productParams) {
  takenProductsAmount++
  titleElem.classList.add('active')
  titleAmountElem.textContent = takenProductsAmount
  titlePreffixElem.textContent = takenProductsAmount == 1 ? 'Bottle' : 'Bottles'
  productsList.insertAdjacentHTML('afterbegin', appendTakenProduct(productParams))

  resetCounter()
}

function counter() {
  counterElem.classList.add('active')
  counterNumbersElem.textContent = timeLeft

  interval = setInterval(() => {
    timeLeft--
    counterNumbersElem.textContent = timeLeft
    if (timeLeft == 0) {
      takeBottleSuccess()
      clearInterval(interval)
    }
  }, 1000)
}

function resetCounter() {
  timeLeft = 60
  // timeLeft = 59;
  // timeLeft = 6;
}

function takeBottleSuccess() {
  unauthorizedUnloadSuccessModal.classList.add('active')
  // mb some more actions
}

// dummy
function takeBottleSimulate() {
  counter()
  takeBottleEvent(firstProduct)

  setTimeout(() => takeBottleEvent(secondProduct), 5000)

  setTimeout(() => {
    takeBottleEvent(thirdProduct)
    timeLeft = 10 // for demo
  }, 10000)
}

takeBottleSimulate()
