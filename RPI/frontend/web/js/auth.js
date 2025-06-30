import { verifyInputsHandler, passwordInputsHandler } from './helpers/helpers'

const registerVerficationModal = document.getElementById('register-verification-modal')
const registerSuccessModal = document.getElementById('register-success-modal')

document.querySelectorAll('[data-target]').forEach((btn) => {
  btn.addEventListener('click', () => {
    const target = document.getElementById(btn.dataset.target)
    target.classList.add('active')

    if (btn.dataset.target == 'register-verification-modal') {
      verifyInputsHandler(registerVerficationModal, registerSuccessModal)
    }
  })
})

passwordInputsHandler('.password-wrapper')
