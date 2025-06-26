import { favoriteHandler } from './helpers/helpers'

const takeWineModal = document.getElementById('take-bottle-drawer-modal')
const takeWineSuccessModal = document.getElementById('take-bottle-success-modal')

const drawerBottles = document.querySelectorAll('.bottle')

drawerBottles.forEach((bottle) => {
  bottle.addEventListener('click', () => {
    drawerBottles.forEach((bottle) => bottle.classList.remove('selected'))
    bottle.classList.add('selected')
  })
})

document.querySelectorAll('[data-target]').forEach((btn) => {
  btn.addEventListener('click', () => {
    const target = document.getElementById(btn.dataset.target)
    target.classList.add('active')

    if (btn.dataset.target == 'take-bottle-drawer-modal') {
      // dummies
      setTimeout(() => {
        btn.closest('.modal').classList.remove('active')
      }, 500)

      setTimeout(() => {
        takeWineSuccessModal.classList.add('active')
      }, 4500)

      setTimeout(() => {
        takeWineModal.classList.remove('active')
      }, 5000)
    }

    if (btn.dataset.target == 'drawer-modal') {
      takeWineModal.classList.remove('active')
      takeWineSuccessModal.classList.remove('active')
    }
  })
})

favoriteHandler('.favorite-btn')
