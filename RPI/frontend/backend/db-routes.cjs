const fs = require('fs/promises')
const path = require('path')
const inventoryPath = path.join(__dirname, '../../database/inventory.json')

async function dbRoutes(fastify) {
    fastify.post('/update-inventory', async (request, reply) => {
        const { mode, target, humidity, zone } = request.body
        try {
            const data = await fs.readFile(inventoryPath, 'utf8')
            const inventory = JSON.parse(data)
            const zoneName = zone.charAt(0).toUpperCase() + zone.slice(1).toLowerCase()
            for (const drawer of Object.values(inventory.drawers)) {
                if (drawer.zone === zoneName) {
                    drawer.mode = mode
                    drawer.temperature = parseInt(target)
                    drawer.humidity = parseInt(humidity)
                }
            }
            inventory.last_updated = new Date().toISOString()
            await fs.writeFile(inventoryPath, JSON.stringify(inventory, null, 2), 'utf8')
            reply.send({ success: true })
        } catch (err) {
            reply.status(500).send({ success: false, error: err.message })
        }
    })
    fastify.get('/inventory', async (req, reply) => {
        try {
            const data = await fs.readFile(inventoryPath, 'utf-8')
            reply.type('application/json').send(JSON.parse(data))
        } catch (err) {
            reply.status(500).send({ error: 'Failed to load inventory' })
        }
    })
}

module.exports = {
    dbRoutes,
}
