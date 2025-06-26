import { getCookie } from './helpers/helpers'

document.querySelectorAll('.modal-overlay').forEach((btn) => {
  btn.addEventListener('click', () => btn.closest('.modal').classList.remove('active'))
})
document.querySelectorAll('.modal-close').forEach((btn) => {
  btn.addEventListener('click', () => btn.closest('.modal').classList.remove('active'))
})
document.querySelectorAll('.all-modals-close-btn').forEach((btn) => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.modal').forEach((modal) => {
      modal.classList.remove('active')
    })
  })
})

// initial color mode
const colorModeState = getCookie('colorModeState')
const html = document.querySelector('html')
const darkModeMediaQuery = window.matchMedia('(prefers-color-scheme: dark)')

function handleSystemColorSchemeChange(event) {
  event.matches ? html.classList.add('color-scheme-system-dark') : html.classList.add('color-scheme-system-light')
}

console.log(window.location.href)

if (colorModeState && window.location.href !== '/account') {
  html.style.setProperty('color-scheme', colorModeState)

  if (colorModeState == 'light') {
    html.classList.add('color-scheme-light')
  } else if (colorModeState == 'dark') {
    html.classList.add('color-scheme-dark')
  } else {
    handleSystemColorSchemeChange(darkModeMediaQuery)
  }
} else {
  html.style.setProperty('color-scheme', 'light')
}
