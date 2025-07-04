const fastify = require('fastify')({ logger: true })
const fastifyStatic = require('@fastify/static')
const path = require('node:path')
const { dbRoutes } = require('./db-routes.cjs')

fastify.register(fastifyStatic, {
  root: path.join(__dirname, '../public'),
  wildcard: true,
  index: ['index.html'],
  extensions: ['html'],
})

fastify.register(dbRoutes)

const port = 3000
const start = async () => {
  try {
    await fastify.listen({ port })
  } catch (err) {
    fastify.log.error(err)
    process.exit(1)
  }
}
start()
