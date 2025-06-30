const swapBottlesModal = document.getElementById('swap-bottles-modal')
const swapBottlesSuccessModal = document.getElementById('swap-bottles-success-modal')

// dummies
const swapGroup = document.querySelector('.swap-group')
const bottleBtns = swapGroup.querySelectorAll('.bottle-placeholder')

const firstText = bottleBtns[0].querySelector('.placeholder-text')
const firstProduct = bottleBtns[0].querySelector('.product-item')
const secondText = bottleBtns[1].querySelector('.placeholder-text')
const secondProduct = bottleBtns[1].querySelector('.product-item')

document.querySelectorAll('[data-target]').forEach((btn) => {
  btn.addEventListener('click', () => {
    const target = document.getElementById(btn.dataset.target)
    target.classList.add('active')

    if (btn.dataset.target == 'swap-bottles-modal') {
      swapBottleScene()
    }
  })
})

function swapBottleScene() {
  swapBottlesSuccessModal.classList.contains('active') && swapBottlesSuccessModal.classList.remove('active')

  // dummies
  setTimeout(() => {
    firstText.classList.remove('active')
    firstProduct.classList.add('active')
  }, 2000)

  setTimeout(() => {
    secondText.classList.remove('active')
    secondProduct.classList.add('active')
  }, 4000)

  setTimeout(() => {
    swapBottlesSuccessModal.classList.add('active')
  }, 6000)

  setTimeout(() => {
    resetScene()
  }, 6500)
}

function resetScene() {
  swapBottlesModal.classList.remove('active')
  firstText.classList.add('active')
  secondText.classList.add('active')
  firstProduct.classList.remove('active')
  secondProduct.classList.remove('active')
}

swapBottleScene()
