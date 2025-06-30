import { passwordInputsHandler } from './helpers/helpers'

document.querySelectorAll('[data-target]').forEach((btn) => {
  btn.addEventListener('click', () => {
    const target = document.getElementById(btn.dataset.target)
    target.classList.add('active')
  })
})

passwordInputsHandler('.password-wrapper')
