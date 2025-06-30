import { verifyInputsHandler, passwordInputsHandler } from './helpers/helpers'

const forgotPasswordVerficationModal = document.getElementById('forgot-password-verification-modal')
const forgotPasswordNewModal = document.getElementById('forgot-password-new-modal')
// const forgotPasswordSuccessModal = document.getElementById('forgot-password-success-modal')

document.querySelectorAll('[data-target]').forEach((btn) => {
  btn.addEventListener('click', () => {
    const target = document.getElementById(btn.dataset.target)
    target.classList.add('active')

    if (btn.dataset.target == 'forgot-password-verification-modal') {
      verifyInputsHandler(forgotPasswordVerficationModal, forgotPasswordNewModal)
    }
  })
})

passwordInputsHandler('.password-wrapper')
