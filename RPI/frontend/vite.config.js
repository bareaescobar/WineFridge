import path from 'path'
import { defineConfig } from 'vite'
import fs from 'fs'
import includeHtml from 'vite-plugin-include-html'
import handlebars from 'vite-plugin-handlebars'
import context from './web/content/inventory.json'
import { viteStaticCopy } from 'vite-plugin-static-copy'

function getHtmlEntryFiles(srcDir) {
  const entry = {}
  function traverseDir(currentDir) {
    const files = fs.readdirSync(currentDir)
    files.forEach((file) => {
      const filePath = path.join(currentDir, file)
      const isDirectory = fs.statSync(filePath).isDirectory()
      if (isDirectory) {
        traverseDir(filePath)
      } else if (path.extname(file) === '.html') {
        const name = path.relative(srcDir, filePath).replace(/\..*$/, '')
        entry[name] = filePath
      }
    })
  }
  traverseDir(srcDir)
  return entry
}

const src = 'web'
export default defineConfig({
  root: `./${src}`,
  base: './',
  plugins: [
    includeHtml(),
    handlebars({
      context,
      helpers: {
        findSelectedFridge: (fridges) => fridges.find((fridge) => fridge.selected),
      },
    }),
    viteStaticCopy({
      targets: [
        {
          src: path.resolve(__dirname, `${src}/img`),
          dest: '.',
        },
      ],
    }),
  ],
  server: {
    open: './index.html',
    host: true,
    watch: {
    ignored: ['**/database/inventory.json'],
  },
  },
  css: {
    devSourcemap: true,
    preprocessorOptions: {
      scss: {
        charset: false,
      },
    },
  },
  build: {
    outDir: '../public',
    assetsDir: './',
    assetsInlineLimit: 4096,
    emptyOutDir: true,
    target: 'es2015',
    rollupOptions: {
      input: getHtmlEntryFiles(src),
      output: {
        manualChunks: undefined,
        assetFileNames: ({ name }) => {
          if (/\.(gif|jpe?g|png|svg)$/.test(name ?? '')) {
            return `[name][extname]`
          }
          return '[name]-[hash][extname]'
        },
      },
    },
  },
})
