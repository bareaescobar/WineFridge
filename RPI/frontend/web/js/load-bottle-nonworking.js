// Simplified load-bottle.js for demo
import { BROKER_URL } from './constants/mqtt-variables.js'
import wineCatalog from '../../../database/wine-catalog.json'

const loadBottleWelcomeModal = document.getElementById('load-bottle-welcome-modal')
const loadBottleInfoModal = document.getElementById('load-bottle-info-modal')
const loadBottleDrawerModal = document.getElementById('load-bottle-drawer-modal')
const loadBottleSuccessModal = document.getElementById('load-bottle-success-modal')
const loadBottleErrorModal = document.getElementById('load-bottle-error-modal')

let mqttClient = null
let scannedBottle = null

// Connect to MQTT over WebSocket
function connectMQTT() {
    mqttClient = mqtt.connect(BROKER_URL, {
        clientId: 'web-load-' + Math.random().toString(16).substr(2, 8)
    })
    
    mqttClient.on('connect', () => {
        console.log('[MQTT] Connected')
        mqttClient.subscribe('winefridge/system/status')
    })
    
    mqttClient.on('message', (topic, message) => {
        try {
            const msg = JSON.parse(message.toString())
            console.log('[MQTT]', msg.action, msg.data)
            handleMQTTMessage(msg)
        } catch (e) {
            console.error('[MQTT] Parse error:', e)
        }
    })
}

// Handle MQTT messages
function handleMQTTMessage(msg) {
    switch(msg.action) {
        case 'barcode_scanned':
            handleBarcodeScanned(msg.data)
            break
            
        case 'expect_bottle':
            handleExpectBottle(msg.data)
            break
            
        case 'bottle_placed':
            handleBottlePlaced(msg.data)
            break
    }
}

// Handle barcode scan
function handleBarcodeScanned(data) {
    const barcode = data.barcode
    const wine = wineCatalog.wines[barcode]
    
    if (wine) {
        scannedBottle = { ...wine, barcode }
        
        // Update modal with wine info
        loadBottleInfoModal.querySelector('.wine-name').textContent = wine.name
        loadBottleInfoModal.querySelector('.wine-type').textContent = wine.type || 'Red Wine'
        loadBottleInfoModal.querySelector('.wine-region').textContent = wine.region || 'Spain'
        
        // Show wine info modal
        loadBottleWelcomeModal.classList.remove('active')
        loadBottleInfoModal.classList.add('active')
    } else {
        // Wine not found
        loadBottleWelcomeModal.classList.remove('active')
        loadBottleErrorModal.classList.add('active')
        setTimeout(() => {
            loadBottleErrorModal.classList.remove('active')
            loadBottleWelcomeModal.classList.add('active')
        }, 3000)
    }
}

// Handle expect bottle
function handleExpectBottle(data) {
    // Update drawer modal
    const drawerText = `Place bottle in Drawer ${data.drawer.replace('drawer_', '')}, Position ${data.position}`
    loadBottleDrawerModal.querySelector('.instruction').textContent = drawerText
    
    // Show drawer modal
    loadBottleInfoModal.classList.remove('active')
    loadBottleDrawerModal.classList.add('active')
}

// Handle bottle placed
function handleBottlePlaced(data) {
    loadBottleDrawerModal.classList.remove('active')
    
    if (data.success) {
        loadBottleSuccessModal.classList.add('active')
        setTimeout(() => {
            loadBottleSuccessModal.classList.remove('active')
            loadBottleWelcomeModal.classList.add('active')
        }, 3000)
    } else {
        loadBottleErrorModal.querySelector('.error-text').textContent = data.error || 'Placement failed'
        loadBottleErrorModal.classList.add('active')
        setTimeout(() => {
            loadBottleErrorModal.classList.remove('active')
            loadBottleWelcomeModal.classList.add('active')
        }, 3000)
    }
    
    scannedBottle = null
}

// Confirm button handler
document.addEventListener('click', (e) => {
    if (e.target.classList.contains('confirm-load-btn')) {
        if (scannedBottle) {
            // Send start_load command
            const message = {
                action: 'start_load',
                source: 'web',
                data: {
                    barcode: scannedBottle.barcode,
                    name: scannedBottle.name
                },
                timestamp: new Date().toISOString()
            }
            
            mqttClient.publish('winefridge/system/command', JSON.stringify(message))
            
            // Show loading
            loadBottleInfoModal.classList.remove('active')
            loadBottleWelcomeModal.classList.add('active')
            loadBottleWelcomeModal.querySelector('.processing').classList.add('active')
        }
    }
})

// Initialize on load
connectMQTT()
