import {
  tabSelectorsHandler,
  wheelSelectorHandler,
  linearProgressHandler,
  codeModalsGroupHandler,
} from './helpers/helpers'

const setupFridgeModal = document.getElementById('setup-fridge-modal')
const setupFridgeZones = setupFridgeModal.querySelectorAll('.zone-item')
const setupUpperZoneModal = document.getElementById('setup-upper-zone-modal')
const setupMiddleZoneModal = document.getElementById('setup-middle-zone-modal')
const setupLowerZoneModal = document.getElementById('setup-lower-zone-modal')

const zoneModalsArray = [setupUpperZoneModal, setupMiddleZoneModal, setupLowerZoneModal]

const setupFridgePincodeModal = document.getElementById('setup-fridge-pincode-modal')
const setupPincodeConfirmModal = document.getElementById('setup-pincode-confirm-modal')
const setupFridgeSuccessModal = document.getElementById('setup-fridge-success-modal')
const setupPincodeModalsArray = [setupFridgePincodeModal, setupPincodeConfirmModal, setupFridgeSuccessModal]

document.querySelectorAll('[data-target]').forEach((btn) => {
  btn.addEventListener('click', () => {
    const target = document.getElementById(btn.dataset.target)
    target.classList.add('active')

    if (btn.dataset.target == 'setup-fridge-pincode-modal') {
      codeModalsGroupHandler(setupPincodeModalsArray)
    }
  })
})

zoneModalsArray.forEach((zoneModal, index) => {
  const saveBtn = zoneModal.querySelector('.btn')

  saveBtn.addEventListener('click', () => {
    setupFridgeZones[index].classList.add('selected')

    index == 2 && setupFridgeModal.querySelector('.btn').removeAttribute('disabled')
  })
})

tabSelectorsHandler('.tabs-group')
wheelSelectorHandler('.wheel-progress-group')
linearProgressHandler('.linear-progress-group')
