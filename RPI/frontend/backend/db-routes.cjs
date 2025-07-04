const fs = require('fs/promises')
const path = require('path')
const inventoryPath = path.join(__dirname, '../../database/inventory.json')
const extractedPath = path.join(__dirname, '../../database/extracted.json')

const EXPIRED_TIME = 3 //hours

async function dbRoutes(fastify) {
  fastify.post('/update-inventory', async (req, reply) => {
    const { mode, target, humidity, zone } = req.body
    try {
      const inventory = await readJSON(inventoryPath)
      const zoneName = formatZoneName(zone)
      for (const drawer of Object.values(inventory.drawers)) {
        if (drawer.zone === zoneName) {
          drawer.mode = mode
          drawer.temperature = parseInt(target)
          drawer.humidity = parseInt(humidity)
        }
      }
      inventory.last_updated = new Date().toISOString()
      await writeJSON(inventoryPath, inventory)
      reply.send({ success: true })
    } catch (err) {
      reply.status(500).send({ success: false, error: err.message })
    }
  })

  fastify.get('/inventory', async (req, reply) => {
    try {
      const data = await readJSON(inventoryPath)
      reply.type('application/json').send(data)
    } catch (err) {
      reply.status(500).send({ error: 'Failed to load inventory' })
    }
  })

  fastify.post('/swap-bottles', async (req, reply) => {
    const { from, to } = req.body
    if (!isValidPosition(from) || !isValidPosition(to)) {
      return reply.status(400).send({ success: false, error: 'Missing swap positions' })
    }
    try {
      const inventory = await readJSON(inventoryPath)
      const drawerFrom = inventory.drawers[from.drawer]
      const drawerTo = inventory.drawers[to.drawer]
      if (!drawerFrom || !drawerTo) {
        return reply.status(404).send({ success: false, error: 'Invalid drawer(s)' })
      }
      const temp = drawerFrom.positions[from.position]
      drawerFrom.positions[from.position] = drawerTo.positions[to.position]
      drawerTo.positions[to.position] = temp
      inventory.last_updated = new Date().toISOString()
      await writeJSON(inventoryPath, inventory)
      reply.send({ success: true })
    } catch (err) {
      reply.status(500).send({ success: false, error: err.message })
    }
  })

  fastify.post('/remove-bottle', async (req, reply) => {
    const { barcode, drawer, position } = req.body
    if (!barcode || !isValidPosition({ drawer, position })) {
      return reply.status(400).send({ success: false, error: 'Invalid payload' })
    }
    try {
      const inventory = await readJSON(inventoryPath)
      const slot = inventory?.drawers?.[drawer]?.positions?.[position]
      if (slot?.occupied && slot.barcode === barcode) {
        inventory.drawers[drawer].positions[position] = { occupied: false }
      }
      inventory.last_updated = new Date().toISOString()
      await writeJSON(inventoryPath, inventory)
      reply.send({ success: true })
    } catch (err) {
      reply.status(500).send({ success: false, error: err.message })
    }
  })

  fastify.post('/add-extracted-bottle', async (req, reply) => {
    const { barcode, drawer, position } = req.body
    if (!barcode || !isValidPosition({ drawer, position })) {
      return reply.status(400).send({ success: false, error: 'Invalid payload' })
    }
    try {
      const extracted = await readJSON(extractedPath)
      if (!extracted[barcode]) {
        extracted[barcode] = { locations: [] }
      }
      const alreadyExists = extracted[barcode].locations.some(
        (loc) => loc.drawer === drawer && loc.position === position,
      )
      if (!alreadyExists) {
        extracted[barcode].locations.unshift({
          drawer,
          position,
          timestamp: new Date().toISOString(),
        })
      }
      for (const code in extracted) {
        extracted[code].locations = filterOldLocations(extracted[code].locations, EXPIRED_TIME)
        if (extracted[code].locations.length === 0) {
          delete extracted[code]
        }
      }
      await writeJSON(extractedPath, extracted)
      reply.send({ success: true })
    } catch (err) {
      reply.status(500).send({ success: false, error: err.message })
    }
  })

  fastify.delete('/remove-extracted-bottle/:barcode', async (req, reply) => {
    const { barcode } = req.params
    if (!barcode) {
      return reply.status(400).send({ success: false, error: 'Barcode is required' })
    }
    try {
      const extracted = await readJSON(extractedPath)
      if (!extracted[barcode]) {
        return reply.status(404).send({ success: false, error: 'Barcode not found' })
      }
      delete extracted[barcode]
      await writeJSON(extractedPath, extracted)
      reply.send({ success: true, message: `Barcode ${barcode} deleted` })
    } catch (err) {
      reply.status(500).send({ success: false, error: err.message })
    }
  })

  fastify.get('/extracted', async (req, reply) => {
    try {
      const data = await readJSON(extractedPath)
      reply.type('application/json').send(data)
    } catch (err) {
      reply.status(500).send({ error: 'Failed to load extracted bottles' })
    }
  })
}

function filterOldLocations(locations, maxAgeHours) {
  const cutoff = Date.now() - maxAgeHours * 60 * 60 * 1000
  return locations.filter((loc) => new Date(loc.timestamp).getTime() > cutoff)
}

async function readJSON(filePath) {
  try {
    const data = await fs.readFile(filePath, 'utf8')
    return JSON.parse(data)
  } catch (err) {
    return {}
  }
}

async function writeJSON(filePath, data) {
  await fs.writeFile(filePath, JSON.stringify(data, null, 2), 'utf8')
}

function isValidPosition(obj) {
  return obj && typeof obj.drawer === 'string' && typeof obj.position === 'number'
}

function formatZoneName(zone) {
  return zone.charAt(0).toUpperCase() + zone.slice(1).toLowerCase()
}

module.exports = {
  dbRoutes,
}
