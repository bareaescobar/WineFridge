import { calculateVolumePercent, capitalize, getOccupiedPositions, getZoneWinesAmount } from './helpers'

//DRAWER-INFO
export function renderDrawerTitle(drawerId) {
  const drawerNum = drawerId.split('_')[1]
  return `Drawer ${drawerNum}`
}

export function renderZoneLabel(zone, mode) {
  const modeLabel = mode.charAt(0).toUpperCase() + mode.slice(1)
  return `${zone} Zone, ${modeLabel} Wine`
}

export function renderBottle(volume = 100, occupied, barcode, selected) {
  if (!occupied) return `<div class="bottle"></div>`

  return `
    <div class="bottle ${selected ? 'selected' : ''}" data-barcode="${barcode}">
      <div class="bottle-image"></div>
      <div class="bottle-volume">
        <div class="bottle-volume-title">${volume}%</div>
        <div class="bottle-volume-left">
          <span style="width: ${volume}%"></span>
        </div>
      </div>
    </div>`
}

export function renderAllBottles(positions) {
  return Object.entries(positions)
    .map(([i, { weight, occupied, barcode }]) => {
      const volume = calculateVolumePercent(weight)
      const isFirstPosition = i === '1'

      return renderBottle(volume, occupied, barcode, isFirstPosition)
    })
    .join('')
}

export function renderWineInfo({ name, description, avg_rating, volume }) {
  const rating = avg_rating ? `<div class="bottle-rating">${avg_rating}</div>` : ''
  return `
    <div class="bottle-title">${name}</div>
    <div class="bottle-subtitle">${description}</div>
    ${rating}
    <div class="bottle-volume">${volume}</div>
  `
}

//DRAWER_ELYPSES
export function renderDrawerEllipses() {
  const cxValues = [
    27.353,
    55.6459,
    83.9409,
    112.235, // top row (4)
    13.2055,
    41.4983,
    69.7934,
    98.0873,
    126.382, // bottom row (5)
  ]
  const cyValues = [
    12.6176,
    12.6176,
    12.6176,
    12.6176, // top
    39.3825,
    39.3825,
    39.3825,
    39.3825,
    39.3825, // bottom
  ]

  const ellipses = cxValues
    .map((cx, i) => {
      const cy = cyValues[i]
      return `<ellipse cx="${cx}" cy="${cy}" rx="12.6176" ry="12.6176"></ellipse>`
    })
    .join('')

  return `
    <svg width="139" height="52" viewBox="0 0 139 52" fill="none" xmlns="http://www.w3.org/2000/svg">
      ${ellipses}
    </svg>
  `
}

//FridgePlacement
export function renderFridgePlacement({ zone, drawer, mode, ellipses }) {
  return `
    <div class="info-group">
      <div class="info-group-title">Fridge Placement</div>
      <div class="info-group-content">
        <div class="info-params">
          <div class="param-item">
            <div class="param-item-value">${zone}</div>
            <div class="param-item-title">Zone</div>
          </div>
          <div class="param-item">
            <div class="param-item-value">${drawer}</div>
            <div class="param-item-title">Drawer</div>
          </div>
          <div class="param-item">
            <div class="drawer-position ${mode}">
                ${ellipses}
            </div>
          </div>
        </div>
      </div>
    </div>
  `
}

export function renderDescriptionBlock(description) {
  return `
    <div class="info-group">
      <div class="info-group-title">Description</div>
      <div class="info-group-content">
        <p>${description}</p>
      </div>
    </div>
  `
}

export function renderWineParamsBlock({ vintage, price, alcohol }) {
  return `
    <div class="info-params">
      <div class="param-item">
        <div class="param-item-value">${vintage}</div>
        <div class="param-item-title">Year</div>
      </div>
      <div class="param-item">
        <div class="param-item-value">${price}</div>
        <div class="param-item-title">Price Range</div>
      </div>
      <div class="param-item">
        <div class="param-item-value">${alcohol}</div>
        <div class="param-item-title">Alcohol %</div>
      </div>
    </div>
  `
}

export function renderDetailsBlock(details) {
  const { vintage, price, alcohol, grape, flavours, meal_type, region, country, winery } = details

  return `
    <div class="info-group">
      <div class="info-group-title">Details</div>
      <div class="info-group-content">
        <div>${renderWineParamsBlock({ vintage, price, alcohol })}</div>

        <p><strong>Grapes:</strong> ${grape}</p>
        <p><b>Flavours:</b> ${flavours}</p>
        <p><strong>Food Pairing:</strong> ${meal_type.join(', ')}</p>
        <p><b>Region:</b> ${region}, ${country}</p>
        <p><strong>About the Winery:</strong> ${winery}</p>
      </div>
    </div>
  `
}

export function renderBottleInfoSection(
  element,
  selectedBottle,
  drawerId,
  zone,
  positions,
  mode,
  shouldRenderFridgePlacement = true,
) {
  const wineInfo = element.querySelector('.bottle-head-info')
  const banner = element.querySelector('.bottle-head-banner img')
  const bottleBody = element.querySelector('.bottle-body')

  wineInfo.innerHTML = renderWineInfo(selectedBottle)
  banner.src = selectedBottle.image_url || ''
  bottleBody.innerHTML = ''

  if (shouldRenderFridgePlacement) {
    const drawerNum = drawerId.split('_')[1]
    const ellipsesHTML = renderDrawerEllipses()
    bottleBody.insertAdjacentHTML(
      'afterbegin',
      renderFridgePlacement({ zone, drawer: drawerNum, mode, ellipses: ellipsesHTML }),
    )
    const elipses = bottleBody.querySelectorAll('.drawer-position ellipse')
    const isActivePositions = Object.values(positions).map(({ barcode }) => barcode === selectedBottle.barcode)
    activateEllipsePositionsSimple(isActivePositions, elipses)
  }
  bottleBody.insertAdjacentHTML('beforeend', renderDescriptionBlock(selectedBottle.description))
  bottleBody.insertAdjacentHTML('beforeend', renderDetailsBlock(selectedBottle))
}

function activateEllipsePositionsSimple(positions, ellipses) {
  //   ellipses.forEach((el) => el.classList.remove('active'))
  positions.forEach((isOccupied, i) => {
    if (isOccupied) {
      ellipses[i].classList.add('active')
    }
  })
}

//HOME
export function formatDrawerName(drawerKey) {
  const match = drawerKey.match(/\d+/)
  return match ? `${match[0]} Drawer` : drawerKey
}

export function renderParamGroup(value, label) {
  return `
    <div class="params-group">
      <div class="params-group-value">${value}</div>
      <div class="params-group-label">${label}</div>
    </div>`
}

export function renderDrawerCapacity(bottlesInDrawer) {
  return Array(9)
    .fill(0)
    .map((_, i) => (i === bottlesInDrawer ? '<br /><span></span>' : '<span></span>'))
    .join('')
}

export function renderDrawerItem(drawerName, bottlesInDrawer) {
  return `
    <a href="/drawers?drawer=${drawerName}" class="drawer-item">
      <div class="drawer-item-info">
        <div class="drawer-item-title">${formatDrawerName(drawerName)}</div>
        <div class="drawer-item-amount">${bottlesInDrawer} Wines</div>
      </div>
      <div class="drawer-item-capacity">
        ${renderDrawerCapacity(bottlesInDrawer)}
      </div>
    </a>`
}

export function renderZoneParams({ mode, wines, temperature, humidity }) {
  return `
    <div class="zone-params">
      ${renderParamGroup(capitalize(mode), 'Type')}
      ${renderParamGroup(wines, 'Wines')}
      ${renderParamGroup(`${temperature}°C`, 'Temp')}
      ${renderParamGroup(`${humidity}%`, 'Humidity')}
    </div>`
}

export function renderZoneTitle(zoneKey) {
  return `
    <div class="zone-title">
      <button class="text-arrow-wide-btn" data-target="zone-params-modal">
        <span class="text">${capitalize(zoneKey)} Zone</span>
      </button>
    </div>`
}

export function renderFridgeCapacity({ current, total = 81 }) {
  const empty = total - current
  const percentage = Math.round((current / total) * 100)

  return `
    <div class="capacity-head">
      <div class="capacity-status"><span>${current}</span>/ ${total} Wines</div>
      <button class="icon-btn grey-btn capacity-settings-btn" data-target="fridge-info-modal">
        <svg class="icon">
          <use href="#icon-settings"></use>
        </svg>
      </button>
    </div>
    <div class="capacity-progress">
      <span class="green" style="width: ${percentage}%"></span>
    </div>
    <div class="capacity-left"><span>${empty}</span> Empty Spaces</div>
  `
}

export function renderZone({ drawers, key }, inventory) {
  const firstDrawer = inventory.drawers[drawers[0]]
  const { mode, temperature, humidity } = firstDrawer
  const wines = getZoneWinesAmount(drawers, inventory)
  const drawersHTML = drawers
    .map((drawerName) => {
      const bottles = getOccupiedPositions(drawerName, inventory)
      return renderDrawerItem(drawerName, bottles)
    })
    .join('')

  return `
    <div class="zone ${mode}">
      ${renderZoneTitle(key)}
      ${renderZoneParams({ mode, wines, temperature, humidity })}
      <div class="zone-drawers">
        ${drawersHTML}
      </div>
    </div>`
}