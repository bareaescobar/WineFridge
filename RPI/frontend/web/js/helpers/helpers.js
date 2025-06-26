import Swiper from 'swiper'
import 'swiper/css'
import inventory from '../../../db/inventory.json'

export function setCookie(cookieName, cookieValue) {
  let d = new Date()
  d.setTime(d.getTime() + 90 * 24 * 60 * 60 * 1000) // 90 days
  document.cookie = cookieName + '=' + cookieValue + ';expires=' + d.toUTCString() + ';path=/'
}

export function getCookie(cookieName) {
  let name = cookieName + '='
  let i,
    c,
    ca = document.cookie.split(';')
  for (i = 0; i < ca.length; i++) {
    c = ca[i]
    while (c.charAt(0) == ' ') c = c.substring(1)
    if (c.indexOf(name) == 0) return c.substring(name.length, c.length)
  }
  return ''
}

export function verifyInputsHandler(inputsModal, nextModal) {
  const firstInput = inputsModal.querySelector('input')
  const inputs = inputsModal.querySelectorAll('input')

  firstInput.focus()

  inputs.forEach((input, index) => {
    input.addEventListener('input', () => {
      input.value = input.value.replace(/\D/g, '').slice(0, 1)

      if (input.value && index < inputs.length - 1) {
        const next = inputs[index + 1]
        next.disabled = false
        next.focus()
      }

      const allFilled = [...inputs].every((inp) => inp.value.length === 1)
      if (allFilled) {
        const code = [...inputs].map((inp) => inp.value).join('')
        onComplete(code)
      }
    })

    input.addEventListener('keydown', (e) => {
      if (e.key === 'Backspace' && !input.value && index > 0) {
        const prev = inputs[index - 1]
        prev.focus()
        prev.value = ''
      }
    })
  })

  function onComplete(code) {
    console.log('code ' + code)
    nextModal.classList.add('active')

    inputs.forEach((input, i) => {
      input.value = ''
      input.disabled = i !== 0
    })
  }
}

export function passwordInputsHandler(allElementsClassName) {
  const passwords = document.querySelectorAll(allElementsClassName)

  passwords.forEach((password) => {
    const passwordInput = password.querySelector('input[type="password"]')
    const dummyInput = password.querySelector('input[type="text"]')
    const togglePasswordBtn = password.querySelector('.toggle-password-btn')

    let isMasked = true

    let actualPassword = ''

    dummyInput.addEventListener('input', function (e) {
      const newValue = e.target.value

      if (newValue.length > actualPassword.length) {
        const addedChar = newValue.slice(-1)
        actualPassword += addedChar
      } else if (newValue.length < actualPassword.length) {
        actualPassword = actualPassword.slice(0, newValue.length)
      }

      passwordInput.value = actualPassword

      if (isMasked) {
        dummyInput.value = '*'.repeat(actualPassword.length)
      }
    })

    togglePasswordBtn.addEventListener('click', () => {
      isMasked = !isMasked

      dummyInput.value = isMasked ? '*'.repeat(actualPassword.length) : actualPassword

      togglePasswordBtn
        .querySelector('[href]')
        .setAttribute('href', isMasked ? '#icon-eyeslash' : '#icon-eyeslash-visible')
    })

    dummyInput.value = ''
    passwordInput.value = ''
  })
}

export function linearProgressHandler(allElementsClassName) {
  const linearProgressRanges = document.querySelectorAll(allElementsClassName)

  linearProgressRanges.forEach((range) => {
    const rangeText = range.querySelector('.linear-progress-value span')
    const rangeInput = range.querySelector('input')
    const rangeProgress = range.querySelector('.input-progress')
    const defaultInputValue = rangeInput.value

    rangeText.innerText = `${defaultInputValue}`
    rangeProgress.style.width = `${defaultInputValue}%`

    rangeInput.addEventListener('input', () => {
      const value = rangeInput.value
      rangeText.innerText = `${value}`
      rangeProgress.style.width = `${value}%`
    })
  })
}

export function wheelSelectorHandler(allElementsClassName) {
  const wheelProgressRanges = document.querySelectorAll(allElementsClassName)

  wheelProgressRanges.forEach((range) => {
    const inputText = range.querySelector('.wheel-progress-value span')
    const input = range.querySelector('input')
    const slider = range.querySelector('.swiper-container')
    const slides = slider.querySelectorAll('.swiper-slide')

    new Swiper(slider, {
      loop: true,
      grabCursor: true,
      slidesPerView: 5,
      initialSlide: Math.floor(slides.length / 2),
      centeredSlides: true,
      slidesOffsetBefore: 0,
      slidesOffsetAfter: 0,
      loopedSlides: slides.length,

      on: {
        afterInit: function () {
          updateInputValue(this)

          setTimeout(() => {
            this.slideToLoop(this.realIndex, 0)
          }, 50)
        },
        slideChangeTransitionEnd: function () {
          updateInputValue(this)
        },
      },
    })

    function updateInputValue(swiper) {
      requestAnimationFrame(() => {
        const slides = swiper.slides
        const index = swiper.activeIndex

        if (!slides || !slides.length || index === undefined) return

        const activeSlide = slides[index]
        if (!activeSlide) return

        const value = activeSlide.textContent.trim()
        input.value = value
        inputText.innerText = `${value}`
      })
    }
  })
}

export function tabSelectorsHandler(allElementsClassName) {
  const tabsGroups = document.querySelectorAll(allElementsClassName)

  tabsGroups.forEach((tabsGroup) => {
    const tabSelectors = tabsGroup.querySelectorAll('.tab-switcher button')
    const tabs = tabsGroup.querySelectorAll('.tab')

    tabSelectors.forEach((btn, index) => {
      btn.addEventListener('click', () => {
        tabSelectors.forEach((btn) => btn.classList.contains('active') && btn.classList.remove('active'))
        tabs.forEach((tab) => tab.classList.contains('active') && tab.classList.remove('active'))
        tabs[index].classList.add('active')
        btn.classList.add('active')
      })
    })
  })
}

export function searchHandler(allElementsClassName) {
  const searchWrappers = document.querySelectorAll(allElementsClassName)

  searchWrappers.forEach((searchWrapper) => {
    const searchOpenBtn = searchWrapper.querySelector('.search-open-btn')
    const searchCloseBtn = searchWrapper.querySelector('.search-close-btn')
    const searchForm = searchWrapper.querySelector('.search-form')

    searchOpenBtn.addEventListener('click', () => {
      searchWrapper.classList.add('active')
      searchForm.querySelector('input').focus()
    })

    searchCloseBtn.addEventListener('click', () => {
      searchWrapper.classList.remove('active')
    })
  })
}

export function favoriteHandler(allElementsClassName) {
  const favoriteBtns = document.querySelectorAll(allElementsClassName)

  favoriteBtns.forEach((favoriteBtn) => {
    favoriteBtn.addEventListener('click', () => {
      favoriteBtn.classList.toggle('active')
    })
  })
}

export function codeModalsGroupHandler(codeModalsArray) {
  let codeState
  let codeGroups = []
  let placeholdersDots = []
  let groupPincodeLength = 0

  codeModalsArray.length == 4 ? (codeState = 'initial') : (codeState = 'enter-new-code')

  codeGroups.push(codeModalsArray[0].querySelector('.code-group'), codeModalsArray[1].querySelector('.code-group'))
  codeModalsArray[2].querySelector('.code-group') && codeGroups.push(codeModalsArray[2].querySelector('.code-group'))

  placeholdersDots.push(
    ...codeModalsArray[0].querySelectorAll('.code-group-placeholder span'),
    ...codeModalsArray[1].querySelectorAll('.code-group-placeholder span'),
  )
  codeModalsArray[2].querySelector('.code-group') &&
    placeholdersDots.push(...codeModalsArray[2].querySelectorAll('.code-group-placeholder span'))

  codeGroups.forEach((codeGroup) => {
    const dials = codeGroup.querySelectorAll('.code-group-dials button:not(.reset-btn)')
    const resetBtn = codeGroup.querySelector('.reset-btn')
    const placeholders = codeGroup.querySelectorAll('.code-group-placeholder span')

    dials.forEach((btn) => {
      btn.addEventListener('click', addPinCodeDigit)
    })

    resetBtn.addEventListener('click', resetGroupPinCode)

    function addPinCodeDigit() {
      groupPincodeLength++

      placeholders[groupPincodeLength - 1].classList.add('active')

      if (groupPincodeLength == 4) {
        showNextDialModal(codeModalsArray)

        setTimeout(() => {
          resetGroupPinCode()
        }, 500)
      }
    }
  })

  function resetGroupPinCode() {
    placeholdersDots.forEach((dot) => dot.classList.remove('active'))
    groupPincodeLength = 0
  }

  function showNextDialModal(codeModalsArray) {
    // old code modals array: new modal, confirm modal, success modal || new code modals array: confirm modal, success modal

    if (codeModalsArray.length == 4) {
      if (codeState == 'initial') {
        codeState = 'enter-new-code'
        codeModalsArray[1].classList.add('active')
      } else if (codeState == 'enter-new-code') {
        codeState = 'confirm-new-code'
        codeModalsArray[2].classList.add('active')
      } else if (codeState == 'confirm-new-code') {
        codeState = 'success'
        codeModalsArray[3].classList.add('active')
      } else if (codeState == 'success') {
        codeState = 'initial'
      }
    }

    if (codeModalsArray.length == 3) {
      if (codeState == 'enter-new-code') {
        codeState = 'confirm-new-code'
        codeModalsArray[1].classList.add('active')
      } else if (codeState == 'confirm-new-code') {
        codeState = 'success'
        codeModalsArray[2].classList.add('active')
      } else if (codeState == 'success') {
        codeState = 'enter-new-code'
      }
    }
  }
}

export function controlsDropdownTogglersHandler(allElementsClassName) {
  const controlsDropdownTogglerBtns = document.querySelectorAll(allElementsClassName)
  const controlsDropdownBtns = document.querySelectorAll('.controls-dropdown button')

  controlsDropdownTogglerBtns.forEach((btn) => {
    btn.addEventListener('click', () => {
      btn.closest('div').querySelector('.controls-dropdown').classList.toggle('active')
    })
  })
  controlsDropdownBtns.forEach((btn) => {
    btn.addEventListener('click', () => {
      btn.dataset.target && btn.closest('.controls-dropdown').classList.remove('active')
    })
  })
}

export function imgInputsHandler(allElementsClassName) {
  const imgInputWrappers = document.querySelectorAll(allElementsClassName)

  imgInputWrappers.forEach((inputWrapper) => {
    const input = inputWrapper.querySelector('input[type="file"]')
    const img = inputWrapper.querySelector('img')

    input.addEventListener('change', () => readURL(input, img))
  })

  function readURL(input, img) {
    if (input.files && input.files[0]) {
      let reader = new FileReader()

      reader.onload = function (e) {
        img.setAttribute('src', e.target.result)
      }

      reader.readAsDataURL(input.files[0])
    }
  }
}

export function renderSuggestionTemplate(products, list, template) {
  list.innerHTML = ''
  products.forEach((product) => {
    const clone = template.content.cloneNode(true)

    clone.querySelector('img').src = product.img
    clone.querySelector('.product-item-title').textContent = product.name
    clone.querySelector('.product-item-volume').textContent = product.volume
    product.avg_rating
      ? (clone.querySelector('.product-item-rating').textContent = product.avg_rating)
      : clone.querySelector('.product-item-rating').remove()

    list.appendChild(clone)
  })
}
export function updateSuggestionTemplate(products, container) {
  container.innerHTML = ''
  products.forEach((product) => {
    const template = document.getElementById('product-template')
    const modal = template.content.cloneNode(true).firstElementChild
    const fields = modal.querySelectorAll('[data-field]')
    fields.forEach((el) => {
      const field = el.dataset.field
      const value = product[field]
      if (el.tagName === 'IMG') {
        el.src = value
      } else {
        value ? (el.textContent = value) : el.remove()
      }
    })
    container.appendChild(modal)
  })
}

export function updateBottleInfoModal(data, container) {
  const template = document.getElementById('bottle-info-modal-template')
  const modal = template.content.cloneNode(true).firstElementChild
  const fields = modal.querySelectorAll('[data-field]')
  const formatters = {
    region_country: (data) => `${data.region}, ${data.country}`,
    meal_type: (data) => data.meal_type.join(', '),
    alcohol: (data) => `${data.alcohol}%`,
  }
  const defaultFormatter = (data, field) => data[field] ?? ''
  fields.forEach((el) => {
    const field = el.dataset.field
    const formatter = formatters[field] || ((data) => defaultFormatter(data, field))
    const value = formatter(data)
    if (el.tagName === 'IMG') {
      el.src = value
    } else {
      value ? (el.textContent = value) : el.remove()
    }
  })

  container.innerHTML = ''
  container.appendChild(modal)
}

export function updateBottleInfoModalWithPosition(data, positions, container) {
  const template = document.getElementById('bottle-info-modal-template-with-position')
  const modal = template.content.cloneNode(true)
  const formatters = {
    region_country: (d) => `${d.region}, ${d.country}`,
    meal_type: (d) => d.meal_type?.join(', '),
    alcohol: (d) => `${d.alcohol}%`,
    zone: (_, extra) => extra?.zone,
    drawer: (_, extra) => extra?.drawer?.replace('drawer_', '') ?? '',
  }
  const defaultFormatter = (d, field) => d[field] ?? ''

  modal.querySelectorAll('[data-field]').forEach((el) => {
    const field = el.dataset.field
    const formatter = formatters[field] || ((d) => defaultFormatter(d, field))
    const extra = ['zone', 'drawer'].includes(field) ? positions[0] : null
    const value = formatter(data, extra)

    if (!value) {
      el.remove()
    } else if (el.tagName === 'IMG') {
      el.src = value
    } else {
      el.textContent = value
    }
  })

  const ellipses = modal.querySelectorAll('.drawer-position ellipse')
  ellipses.forEach((e) => e.classList.remove('active'))

  positions.forEach((pos) => {
    const index = Number(pos.position) - 1
    if (ellipses[index]) {
      ellipses[index].classList.add('active')
    }
  })

  container.innerHTML = ''
  container.appendChild(modal)
}

export function getBottleDetails(barcode) {
  return Object.entries(inventory.drawers)
    .flatMap(([drawer, data]) =>
      Object.entries(data.positions).map(([position, bottleInfo]) => ({
        drawer,
        position,
        barcode: bottleInfo.barcode,
        name: bottleInfo.name,
        zone: data.zone,
      })),
    )
    .filter((b) => b.barcode === barcode)
}

export function handleMQTTMessage(rawMessage, mqttActions) {
  try {
    const { action, data } = JSON.parse(rawMessage)
    mqttActions[action](data)
  } catch (error) {
    if (!client) throw new Error('Failed to handle MQTT message:', error)
  }
}

export function getOccupiedPositions(drowerIndex) {
  return Object.values(inventory.drawers[drowerIndex].positions).filter((pos) => pos.occupied).length
}

export function getZoneWinesAmount(drawers) {
  return drawers.reduce((sum, drawer) => sum + getOccupiedPositions(drawer), 0)
}

export function getZoneFormData(modalElement) {
  const activeTab = modalElement.querySelector('.tab.active')
  const tabSwitcherActive = modalElement.querySelector('.tab-switcher button.active')
  if (tabSwitcherActive.textContent === 'Automatically') {
    const selectedRadio = activeTab.querySelector('input[type="radio"]:checked')
    const radioLabel = selectedRadio.closest('.radio-label')
    const wineType = radioLabel.querySelector('.radio-title')?.textContent.trim()
    const subtitle = radioLabel.querySelector('.radio-subtitle')?.textContent.trim()
    const { temperature, humidity } = parseWineInfo(subtitle)
    return {
      mode: wineType.split(' ')[0].toLowerCase(),
      temperature,
      humidity,
    }
  } else if (tabSwitcherActive.textContent === 'Manually') {
    const tempValue = activeTab.querySelector('.zone-temperature-group .wheel-progress-value span')?.textContent.trim()
    const humidityValue = activeTab
      .querySelector('.zone-humidity-group .linear-progress-value span')
      ?.textContent.trim()
    return {
      mode: 'manual',
      temperature: parseInt(tempValue, 10),
      humidity: parseInt(humidityValue, 10),
    }
  }
}

export function parseWineInfo(infoString) {
  const result = {}
  const tempMatch = infoString.match(/(\d+)\s*°C/i)
  if (tempMatch) {
    result.temperature = parseInt(tempMatch[1], 10)
  }
  const humidityMatch = infoString.match(/(\d+)\s*%.*?humidity/i)
  if (humidityMatch) {
    result.humidity = parseInt(humidityMatch[1], 10)
  }
  return result
}

export function fetchSync(url) {
  const xhr = new XMLHttpRequest()
  const cacheBuster = `?_=${Date.now()}`
  xhr.open('GET', url + cacheBuster, false)
  try {
    xhr.send()
    if (xhr.status !== 200) throw new Error(`HTTP error! Status: ${xhr.status}`)
    return JSON.parse(xhr.responseText)
  } catch (error) {
    throw error
  }
}
