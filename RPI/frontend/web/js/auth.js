import { verifyInputsHandler, passwordInputsHandler } from './helpers/helpers'
import { connectMQTT, publish } from './mqttClient'
import { TOPICS } from './constants/topics'
import { BROKER_URL } from './constants/mqtt-variables'

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

// ==================== SHUTDOWN ON GOOGLE SIGN-IN ====================
// Connect MQTT for shutdown command
connectMQTT({
  host: BROKER_URL,
  options: { clientId: 'auth-shutdown-client' }
})

// Wait for DOM to be ready
setTimeout(() => {
  // Detectar botón por icono Google
  const buttons = document.querySelectorAll('.social-group-btns .btn')
  let googleSignInBtn = null
  
  buttons.forEach(btn => {
    const svg = btn.querySelector('svg use')
    if (svg && svg.getAttribute('href').includes('logo-google')) {
      googleSignInBtn = btn
    }
  })
  
  console.log('[AUTH] Google button found:', !!googleSignInBtn)
  
  if (googleSignInBtn) {
    googleSignInBtn.addEventListener('click', (e) => {
      e.preventDefault()
      console.log('[SHUTDOWN] Sending shutdown command...')
      
      const payload = {
        action: 'shutdown',
        source: 'web',
        timestamp: new Date().toISOString()
      }
      
      publish(TOPICS.WEB_TO_RPI_COMMAND, JSON.stringify(payload))
      console.log('[SHUTDOWN] Command sent')
      
      alert('System shutting down safely... Please wait 30 seconds before unplugging.')
    })
    console.log('[AUTH] Shutdown listener attached')
  } else {
    console.error('[AUTH] Google button NOT found')
  }
}, 1000)

passwordInputsHandler('.password-wrapper')
