import Swiper from 'swiper'
// import Swiper from 'swiper/bundle';
// import { Navigation, Pagination } from 'swiper/modules';
// import 'swiper/css/bundle';
import 'swiper/css'
// import 'swiper/css/navigation';
// import 'swiper/css/pagination';

const fridgeSelectorModal = document.getElementById('fridge-selector-modal')

// unload bottle group
const unloadBottleWelcomeModal = document.getElementById('unload-bottle-welcome-modal')
const unloadBottleSuggestModal = document.getElementById('unload-bottle-suggest-modal')
const unloadBottleManuallyModal = document.getElementById('unload-bottle-manually-modal')
const mealRecommendModal = document.getElementById('meal-recommend-modal')
const unloadBottleInfoModal = document.getElementById('unload-bottle-info-modal')
const takeWineModal = document.getElementById('take-bottle-drawer-modal')
const takeWineSuccessModal = document.getElementById('take-bottle-success-modal')

// swap group
const swapBottlesModal = document.getElementById('swap-bottles-modal')
const swapBottlesSuccessModal = document.getElementById('swap-bottles-success-modal')

// fridge pincode
const fridgePincodeOldModal = document.getElementById('fridge-pincode-old-modal')
const fridgePincodeNewModal = document.getElementById('fridge-pincode-new-modal')
const fridgePincodeConfirmModal = document.getElementById('fridge-pincode-confirm-modal')
const fridgePincodeSuccessModal = document.getElementById('fridge-pincode-success-modal')
const fridgePincodeModalsArray = [
  fridgePincodeOldModal,
  fridgePincodeNewModal,
  fridgePincodeConfirmModal,
  fridgePincodeSuccessModal,
]

// fridge setup
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

// scan & load bottle
const loadBottleWelcomeModal = document.getElementById('load-bottle-welcome-modal')
const scanBottleErrorModal = document.getElementById('scan-bottle-error-modal')
const loadBottleInfoModal = document.getElementById('load-bottle-info-modal')
const loadBottleDrawerModal = document.getElementById('load-bottle-drawer-modal')
const loadBottleSuccessModal = document.getElementById('load-bottle-success-modal')
const loadBottleErrorModal = document.getElementById('load-bottle-error-modal')

// auth
const registerVerficationModal = document.getElementById('register-verification-modal')
const registerSuccessModal = document.getElementById('register-success-modal')

// forgot password
const forgotPasswordVerficationModal = document.getElementById('forgot-password-verification-modal')
const forgotPasswordNewModal = document.getElementById('forgot-password-new-modal')
const forgotPasswordSuccessModal = document.getElementById('forgot-password-success-modal')

// unauth unload
const unauthorizedUnloadModal = document.getElementById('unauthorized-unload-modal')
const unauthorizedUnloadSuccessModal = document.getElementById('unauthorized-unload-success-modal')

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

document.querySelectorAll('[data-target]').forEach((btn) => {
  btn.addEventListener('click', () => {
    const target = document.getElementById(btn.dataset.target)
    target.classList.add('active')

    if (btn.dataset.target == 'meal-recommend-modal') {
      const btnText = btn.querySelector('.suggest-item-title').textContent
      const modalTitleSelectedTextElem = mealRecommendModal.querySelector('.modal-title span')
      const modalSubitleSelectedTextElem = mealRecommendModal.querySelector('.modal-subtitle span')

      modalTitleSelectedTextElem.textContent = btnText
      modalSubitleSelectedTextElem.textContent = btnText
    }

    if (btn.dataset.target == 'take-bottle-drawer-modal') {
      setTimeout(() => {
        btn.closest('.modal').classList.remove('active')
      }, 500)

      setTimeout(() => {
        // takeWineModal.classList.remove('active');
        takeWineSuccessModal.classList.add('active')
      }, 4500)

      setTimeout(() => {
        takeWineModal.classList.remove('active')
        // takeWineSuccessModal.classList.add('active');
      }, 5000)
    }

    if (btn.dataset.target == 'unload-bottle-welcome-modal') {
      unloadBottleSuggestModal.classList.contains('active') && unloadBottleSuggestModal.classList.remove('active')
      unloadBottleManuallyModal.classList.contains('active') && unloadBottleManuallyModal.classList.remove('active')
      mealRecommendModal.classList.contains('active') && mealRecommendModal.classList.remove('active')
      takeWineSuccessModal.classList.contains('active') && takeWineSuccessModal.classList.remove('active')
    }

    if (btn.dataset.target == 'swap-bottles-modal') {
      // console.log('swap start');
      const swapGroup = document.querySelector('.swap-group')
      const bottleBtns = swapGroup.querySelectorAll('.bottle-placeholder')

      const firstText = bottleBtns[0].querySelector('.placeholder-text')
      const firstProduct = bottleBtns[0].querySelector('.product-item')
      const secondText = bottleBtns[1].querySelector('.placeholder-text')
      const secondProduct = bottleBtns[1].querySelector('.product-item')

      // firstProduct.classList.contains('active')
      //     && firstProduct.classList.remove('active');

      // secondProduct.classList.contains('active')
      //     && secondProduct.classList.remove('active');

      // ! firstText.classList.contains('active')
      //     && firstText.classList.add('active');

      // ! secondText.classList.contains('active')
      //     && secondText.classList.add('active');

      swapBottlesSuccessModal.classList.contains('active') && swapBottlesSuccessModal.classList.remove('active')

      // if (swapBottlesSuccessModal.classList.contains('active')) {
      //     setTimeout(() => {
      //         swapBottlesSuccessModal.classList.remove('active');
      //     }, 100);
      // }

      // заглушки превью
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
        swapBottlesModal.classList.remove('active')
        firstText.classList.add('active')
        secondText.classList.add('active')
        firstProduct.classList.remove('active')
        secondProduct.classList.remove('active')
      }, 6500)
    }

    if (btn.dataset.target == 'fridge-pincode-old-modal') {
      сodeModalsGroupHandler(fridgePincodeModalsArray)
    }

    if (btn.dataset.target == 'setup-fridge-pincode-modal') {
      сodeModalsGroupHandler(setupPincodeModalsArray)
    }

    if (btn.dataset.target == 'load-bottle-welcome-modal') {
      scanBottleScene()
    }

    if (btn.dataset.target == 'load-bottle-drawer-modal') {
      setTimeout(() => {
        // dummy
        // loadBottleSuccessModal.classList.add('active')

        // dummy scan random show, delete after
        Math.random() > 0.5
          ? loadBottleSuccessModal.classList.add('active')
          : loadBottleErrorModal.classList.add('active')
      }, 3000)

      setTimeout(() => {
        // btn.closest('.modal').classList.remove('active')
        loadBottleDrawerModal.classList.remove('active')
      }, 4000)
    }

    if (btn.dataset.target == 'register-verification-modal') {
      // const firstInput = registerVerficationModal.querySelector('input');
      // firstInput.focus();

      verifyInputsHandler(registerVerficationModal, registerSuccessModal)
    }

    if (btn.dataset.target == 'forgot-password-verification-modal') {
      verifyInputsHandler(forgotPasswordVerficationModal, forgotPasswordNewModal)
    }
  })
})

// settings dropdown controls
const controlsDropdownTogglerBtns = document.querySelectorAll('.controls-dropdown-toggler-btn')
controlsDropdownTogglerBtns.forEach((btn) => {
  btn.addEventListener('click', () => {
    btn.closest('div').querySelector('.controls-dropdown').classList.toggle('active')
  })
})

const dropdownControlsBtns = document.querySelectorAll('.controls-dropdown button')
dropdownControlsBtns.forEach((btn) => {
  btn.addEventListener('click', () => {
    // btn.closest('.controls-dropdown').classList.remove('active')
    btn.dataset.target && btn.closest('.controls-dropdown').classList.remove('active')
  })
})

// brightness & humidity ranges
const linearProgressRanges = document.querySelectorAll('.linear-progress-group')
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
// temperature ranges
const wheelProgressRanges = document.querySelectorAll('.wheel-progress-group')
wheelProgressRanges.forEach((range) => {
  const inputText = range.querySelector('.wheel-progress-value span')
  const input = range.querySelector('input')
  const slider = range.querySelector('.swiper-container')
  const slides = slider.querySelectorAll('.swiper-slide')

  // const defaultInputValue = rangeInput.value
  // rangeText.innerText = `${defaultInputValue}`

  // const swiper = new Swiper(slider, {
  //     loop: true,
  //     // speed: 20,
  //     grabCursor: true,
  //     // mousewheelControl: true,
  //     // mousewheel: true,
  //     slidesPerView: 5,
  //     // slidesPerView: 'auto', //not working
  //     initialSlide: Math.ceil(slides.length / 2),
  //     // initialSlide: 5,
  //     // activeIndex: 5,
  //     // spaceBetween: 40,
  //     // freeMode: true,
  //     // freeModeMomentum: true,
  //     // freeModeSticky: true,
  //     centeredSlides: true,
  //     // additionalSlides: 3,
  //     // loopAdditionalSlides: 6,
  //     loopAdditionalSlides: 2,
  //     slideToClickedSlide: true,
  //     slidesOffsetBefore: 0,
  //     slidesOffsetAfter: 0,

  //     on: {
  //         slideChangeTransitionEnd: function () {
  //         // slideChange: function () {
  //             updateInputValue(this);
  //             // console.log('active index ' + this.activeIndex);
  //             // console.log('realIndex ' +this.realIndex);
  //         },
  //         afterInit: function () {
  //             updateInputValue(this);
  //             // setTimeout(() => updateInputValue(this), 0);
  //             // console.log(this.activeIndex);
  //             // console.log('active index ' + this.activeIndex);
  //             // console.log('realIndex ' +this.realIndex);
  //         }
  //     }
  // });

  // // function updateInputValue(swiperInstance) {

  // //     const activeSlide = swiperInstance.slides[swiperInstance.activeIndex];
  // //     // console.log('active index ' + swiperInstance.activeIndex);
  // //     // console.log('realIndex ' + swiperInstance.realIndex);
  // //     // console.log(swiperInstance.slides);

  // //     // console.log(activeSlide.textContent)

  // //     const value = activeSlide.textContent.trim();
  // //     input.value = value;
  // //     inputText.innerText = `${value}`
  // //     console.log(value);
  // // }

  // function updateInputValue(swiper) {
  //     requestAnimationFrame(() => {
  //         const slides = swiper.slides;
  //         const index = swiper.activeIndex;

  //         if (!slides || !slides.length || index === undefined) return;

  //         const activeSlide = slides[index];
  //         if (!activeSlide) return;

  //         const value = activeSlide.textContent.trim();
  //         input.value = value;
  //         inputText.innerText = `${value}`
  //         console.log(value);
  //     });
  // }
  const swiper = new Swiper(slider, {
    loop: true,
    grabCursor: true,
    slidesPerView: 5,
    initialSlide: Math.floor(slides.length / 2),
    centeredSlides: true,
    // loopAdditionalSlides: 2,
    // slideToClickedSlide: true,
    slidesOffsetBefore: 0,
    slidesOffsetAfter: 0,
    loopedSlides: slides.length,

    on: {
      // init: function () {
      //     console.log('Swiper init');
      // },
      afterInit: function () {
        // console.log('Swiper afterInit');
        updateInputValue(this)

        setTimeout(() => {
          this.slideToLoop(this.realIndex, 0)
        }, 50)
      },
      // slideChange: function () {
      //     console.log('slideChange', this.realIndex);
      // },
      // slideChangeTransitionStart: function () {
      //     console.log('transitionStart');
      // },
      slideChangeTransitionEnd: function () {
        // console.log('transitionEnd');
        updateInputValue(this)
      },
      // reachBeginning: function () {
      //     console.log('Reached beginning');
      // },
      // reachEnd: function () {
      //     console.log('Reached end');
      // },
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
      // console.log(`Updated input: ${value}`);
    })
  }
})

// tabs
const tabsGroups = document.querySelectorAll('.tabs-group')
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

// search
const searchWrappers = document.querySelectorAll('.search-wrapper')
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

// favorite bottle
const favoriteBtn = document.querySelector('.favorite-btn')
favoriteBtn.addEventListener('click', () => {
  favoriteBtn.classList.toggle('active')
})

// drawer bottle selector
const drawers = document.querySelectorAll('.drawer-bottles')
drawers.forEach((drawer) => {
  const bottles = drawer.querySelectorAll('.bottle')
  bottles.forEach((bottle) => {
    bottle.addEventListener('click', () => {
      bottles.forEach((bottle) => bottle.classList.remove('selected'))
      bottle.classList.add('selected')
    })
  })
})

// dials

function сodeModalsGroupHandler(codeModalsArray) {
  // const codeGroups = codeModalsArray.querySelectorAll('.code-group');
  // let codeState = 'initial';
  let codeState
  let codeGroups = []
  let placeholdersDots = []

  codeModalsArray.length == 4 ? (codeState = 'initial') : (codeState = 'enter-new-code')

  codeGroups.push(
    codeModalsArray[0].querySelector('.code-group'),
    codeModalsArray[1].querySelector('.code-group'),
    // codeModalsArray[2].querySelector('.code-group') && codeModalsArray[2].querySelector('.code-group'),
  )
  codeModalsArray[2].querySelector('.code-group') && codeGroups.push(codeModalsArray[2].querySelector('.code-group'))

  placeholdersDots.push(
    ...codeModalsArray[0].querySelectorAll('.code-group-placeholder span'),
    ...codeModalsArray[1].querySelectorAll('.code-group-placeholder span'),
    // ...(codeModalsArray[2].querySelector('.code-group') &&
    //     codeModalsArray[2].querySelectorAll('.code-group-placeholder span')),
  )
  codeModalsArray[2].querySelector('.code-group') &&
    placeholdersDots.push(...codeModalsArray[2].querySelectorAll('.code-group-placeholder span'))

  // console.log(placeholdersDots);

  codeGroups.forEach((codeGroup) => {
    // let pinCodeFullState = false;
    let pinCodeLength = 0
    // let pinCodeState = 'enter-old-pin';

    const dials = codeGroup.querySelectorAll('.code-group-dials button:not(.reset-btn)')
    const resetBtn = codeGroup.querySelector('.reset-btn')
    const placeholders = codeGroup.querySelectorAll('.code-group-placeholder span')

    dials.forEach((btn) => {
      btn.addEventListener('click', addPinCodeDigit)
    })

    resetBtn.addEventListener('click', resetPinCode)

    function addPinCodeDigit() {
      pinCodeLength++
      placeholders[pinCodeLength - 1].classList.add('active')

      if (pinCodeLength == 4) {
        // console.log('full pin code');
        // resetPinCode();
        showNextDialModal(codeModalsArray)
      }
    }

    function resetPinCode() {
      placeholders.forEach((dot) => dot.classList.remove('active'))
      pinCodeLength = 0
    }
  })

  function resetDotsAfterSuccess() {
    // console.log('reset dots after success');
    placeholdersDots.forEach((dot) => dot.classList.remove('active'))
  }

  function showNextDialModal(codeModalsArray) {
    // old code modal - new - confirm - success || new code - confirm - success

    // console.log('modals amount is ' + codeModalsArray.length);
    // codeModalsArray.length == 4 ? codeState = 'initial' : codeState = 'enter-new-code';

    if (codeModalsArray.length == 4) {
      if (codeState == 'initial') {
        codeState = 'enter-new-code'
        codeModalsArray[1].classList.add('active')
      } else if (codeState == 'enter-new-code') {
        codeState = 'confirm-new-code'
        codeModalsArray[2].classList.add('active')
      } else if (codeState == 'confirm-new-code') {
        codeState = 'success'
        resetDotsAfterSuccess()
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
        resetDotsAfterSuccess()
        codeModalsArray[2].classList.add('active')
      } else if (codeState == 'success') {
        codeState = 'enter-new-code'
      }
    }

    // if (codeState == 'initial') {
    //     codeState = 'enter-new-code';
    //     codeModalsArray[1].classList.add('active');
    // }
    // else if (codeState == 'enter-new-code') {
    //     codeState = 'confirm-new-code';
    //     codeModalsArray[2].classList.add('active');
    // }
    // else if (codeState == 'confirm-new-code') {
    //     codeState = 'success';
    //     codeModalsArray[3].classList.add('active');
    // }
    // else if (codeState == 'success') {
    //     codeState = 'initial';
    // }

    // console.log(codeState);
  }
}

function setCookie(cookieName, cookieValue) {
  let d = new Date()
  d.setTime(d.getTime() + 90 * 24 * 60 * 60 * 1000) // 90 days
  document.cookie = cookieName + '=' + cookieValue + ';expires=' + d.toUTCString() + ';path=/'
}

function getCookie(cookieName) {
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

// color theme
const themeGroup = document.querySelector('.theme-group-btn')
if (themeGroup) {
  const darkModeMediaQuery = window.matchMedia('(prefers-color-scheme: dark)')
  const html = document.querySelector('html')
  const themeBtns = themeGroup.querySelectorAll('.controls-dropdown-btn')
  const colorModeTextElem = themeGroup.querySelector('.colormode-value-text')

  const colorModeState = getCookie('colorModeState')
  // console.log(colorModeState)

  if (colorModeState) {
    html.style.setProperty('color-scheme', colorModeState)
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
        // html.removeAttribute('class')
        // handleColorSchemeChange(darkModeMediaQuery)
        break
    }

    // handleColorSchemeChange(darkModeMediaQuery)
  } else {
    html.style.setProperty('color-scheme', 'light')
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

      // console.log(colorModeText);

      setCookie('colorModeState', colorMode)

      switchColorMode(colorMode)
      switchSelectedModeText(colorModeText)
    })
  })

  function switchSelectedModeText(val) {
    if (val == 'light' || val == 'white' || val == 'White') colorModeTextElem.textContent = 'White'
    else if (val == 'dark' || val == 'Dark') colorModeTextElem.textContent = 'Dark'
    else colorModeTextElem.textContent = 'System'
  }

  function switchColorMode(colorMode) {
    if (colorMode == 'light') {
      html.style.setProperty('color-scheme', 'light')
      setCookie('colorModeState', 'light')
      themeBtns[0].classList.add('checked')

      // html.removeAttribute('class')
      html.classList.contains('color-scheme-dark') && html.classList.remove('color-scheme-dark')
      html.classList.contains('color-scheme-system-light') && html.classList.remove('color-scheme-system-light')
      html.classList.contains('color-scheme-system-dark') && html.classList.remove('color-scheme-system-dark')
      html.classList.add('color-scheme-light')
    } else if (colorMode == 'dark') {
      html.style.setProperty('color-scheme', 'dark')
      setCookie('colorModeState', 'dark')
      themeBtns[1].classList.add('checked')

      // html.removeAttribute('class')
      html.classList.contains('color-scheme-light') && html.classList.remove('color-scheme-light')
      html.classList.contains('color-scheme-system-light') && html.classList.remove('color-scheme-system-light')
      html.classList.contains('color-scheme-system-dark') && html.classList.remove('color-scheme-system-dark')
      html.classList.add('color-scheme-dark')
    } else {
      html.style.setProperty('color-scheme', 'light dark')
      setCookie('colorModeState', 'light dark')
      themeBtns[2].classList.add('checked')

      // html.removeAttribute('class')
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

// passwords
const passwords = document.querySelectorAll('.password-wrapper')
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

zoneModalsArray.forEach((zoneModal, index) => {
  const saveBtn = zoneModal.querySelector('.btn')

  saveBtn.addEventListener('click', () => {
    setupFridgeZones[index].classList.add('selected')

    index == 2 && setupFridgeModal.querySelector('.btn').removeAttribute('disabled')
  })
})

// unload image
const imgInputWrappers = document.querySelectorAll('.img-file-wrapper')
if (imgInputWrappers) {
  imgInputWrappers.forEach((inputWrapper) => {
    const input = inputWrapper.querySelector('input[type="file"]')
    const img = inputWrapper.querySelector('img')

    input.addEventListener('change', readURL(input, img))
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

// scan bottle scene
const scanBtn = loadBottleWelcomeModal.querySelector('.btn')
const scanTitle = loadBottleWelcomeModal.querySelector('.modal-head-title')
const scanImg = loadBottleWelcomeModal.querySelector('.full')
const scanCircle = loadBottleWelcomeModal.querySelector('.processing')

let scanButtonHandlerAttached = false

function scanBottleScene() {
  setTimeout(() => scanImg.classList.add('active'), 2000)
  setTimeout(() => (scanTitle.style.opacity = 1), 2500)
  setTimeout(() => scanBtn.classList.add('active'), 3500)

  scanBottleErrorModal.classList.remove('active')
  loadBottleInfoModal.classList.remove('active')

  if (!scanButtonHandlerAttached) {
    scanBtn.addEventListener('click', () => {
      scanCircle.classList.add('active')

      // dummy scan event
      setTimeout(() => {
        if (Math.random() > 0.5) {
          loadBottleInfoModal.classList.add('active')
        } else {
          scanBottleErrorModal.classList.add('active')
        }
      }, 2000)

      setTimeout(() => {
        resetBottleScene()
        // scanBtn.closest('.modal').classList.remove('active');
        loadBottleWelcomeModal.classList.remove('active')
      }, 3000)
    })

    scanButtonHandlerAttached = true
  }
}

function resetBottleScene() {
  scanTitle.style.opacity = 0
  scanImg.classList.remove('active')
  scanBtn.classList.remove('active')
  scanCircle.classList.remove('active')
}

// auth
function verifyInputsHandler(inputsModal, nextModal) {
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

    // inputs[0].focus();
  }
}

if (unauthorizedUnloadModal) {
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
    imgUrl: 'img/product-1.png',
    title: 'Amberwood Merlot',
    rating: '4.7',
    volume: '400ml',
  }

  const secondProduct = {
    imgUrl: 'img/product-2.png',
    title: 'Velvet Noir Reserve',
    rating: '4.0',
    volume: '400ml',
  }

  const thirdProduct = {
    imgUrl: 'img/product-3.png',
    title: 'Golden Harvest',
    rating: '3.9',
    volume: '400ml',
  }

  const fourthProduct = {
    imgUrl: 'img/product-4.png',
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
    productsList.innerHTML += appendTakenProduct(productParams)

    resetCounter()
  }

  function counter() {
    counterElem.classList.add('active')
    counterNumbersElem.textContent = timeLeft

    interval = setInterval(() => {
      timeLeft--
      counterNumbersElem.textContent = timeLeft
      if (timeLeft == 0) {
        // console.log('success');
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
  function takeBottlESimulate() {
    counter()
    takeBottleEvent(firstProduct)

    // setTimeout(() => {
    // }, 5000);

    setTimeout(() => takeBottleEvent(secondProduct), 4000)

    setTimeout(() => takeBottleEvent(thirdProduct), 8000)

    setTimeout(() => takeBottleEvent(fourthProduct), 12000)
  }
  // takeBottlESimulate();
}
