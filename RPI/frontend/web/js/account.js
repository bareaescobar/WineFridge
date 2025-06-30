import {
  getCookie,
  setCookie,
  controlsDropdownTogglersHandler,
  passwordInputsHandler,
  searchHandler,
} from './helpers/helpers'

document.querySelectorAll('[data-target]').forEach((btn) => {
  btn.addEventListener('click', () => {
    const target = document.getElementById(btn.dataset.target)
    target.classList.add('active')
  })
})

const themeGroup = document.querySelector('.theme-group-btn')
const colorModeState = getCookie('colorModeState')
const darkModeMediaQuery = window.matchMedia('(prefers-color-scheme: dark)')
const html = document.querySelector('html')
const themeBtns = themeGroup.querySelectorAll('.controls-dropdown-btn')
const colorModeTextElem = themeGroup.querySelector('.colormode-value-text')

if (themeGroup) {
  if (colorModeState) {
    switchSelectedModeText(colorModeState)

    switch (colorModeState) {
      case 'light':
        themeBtns[0].classList.add('checked')
        html.classList.add('color-scheme-light')
        break
      case 'dark':
        themeBtns[1].classList.add('checked')
        html.classList.add('color-scheme-dark')
        break
      case 'light dark':
        themeBtns[2].classList.add('checked')
        break
    }
  } else {
    switchSelectedModeText('light')
    themeBtns[0].classList.add('checked')
  }

  function handleSystemColorSchemeChange(event) {
    event.matches ? html.classList.add('color-scheme-system-dark') : html.classList.add('color-scheme-system-light')
  }

  themeBtns.forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault()

      themeBtns.forEach((btn) => btn.classList.remove('checked'))
      btn.classList.add('checked')

      const colorMode = btn.dataset.colormode
      const colorModeText = btn.querySelector('span').textContent

      setCookie('colorModeState', colorMode)

      switchColorMode(colorMode)
      switchSelectedModeText(colorModeText)
    })
  })

  function switchSelectedModeText(textValue) {
    if (textValue == 'light' || textValue == 'white' || textValue == 'White') colorModeTextElem.textContent = 'White'
    else if (textValue == 'dark' || textValue == 'Dark') colorModeTextElem.textContent = 'Dark'
    else colorModeTextElem.textContent = 'System'
  }

  function switchColorMode(colorMode) {
    if (colorMode == 'light') {
      html.style.setProperty('color-scheme', 'light')
      setCookie('colorModeState', 'light')
      themeBtns[0].classList.add('checked')

      html.classList.contains('color-scheme-dark') && html.classList.remove('color-scheme-dark')
      html.classList.contains('color-scheme-system-light') && html.classList.remove('color-scheme-system-light')
      html.classList.contains('color-scheme-system-dark') && html.classList.remove('color-scheme-system-dark')
      html.classList.add('color-scheme-light')
    } else if (colorMode == 'dark') {
      html.style.setProperty('color-scheme', 'dark')
      setCookie('colorModeState', 'dark')
      themeBtns[1].classList.add('checked')

      html.classList.contains('color-scheme-light') && html.classList.remove('color-scheme-light')
      html.classList.contains('color-scheme-system-light') && html.classList.remove('color-scheme-system-light')
      html.classList.contains('color-scheme-system-dark') && html.classList.remove('color-scheme-system-dark')
      html.classList.add('color-scheme-dark')
    } else {
      html.style.setProperty('color-scheme', 'light dark')
      setCookie('colorModeState', 'light dark')
      themeBtns[2].classList.add('checked')

      html.classList.contains('color-scheme-light') && html.classList.remove('color-scheme-light')
      html.classList.contains('color-scheme-dark') && html.classList.remove('color-scheme-dark')
      handleSystemColorSchemeChange(darkModeMediaQuery)
    }
  }

  document.addEventListener('click', (e) => {
    if (!themeGroup.contains(e.target)) {
      themeGroup.querySelector('.controls-dropdown').classList.remove('active')
    }
  })
}

controlsDropdownTogglersHandler('.controls-dropdown-toggler-btn')
passwordInputsHandler('.password-wrapper')
searchHandler('.search-wrapper')
