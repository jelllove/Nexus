import * as esbuild from 'esbuild'
import path from 'path'
import { fileURLToPath } from 'url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))

await esbuild.build({
    entryPoints: [path.join(__dirname, 'entry.js')],
    bundle: true,
    format: 'iife',
    outfile: path.join(__dirname, '..', 'resources', 'editor', 'tiptap-bundle.js'),
    minify: true,
    target: ['chrome108'],  // Qt 6.8 uses Chromium 122, but be safe
})

console.log('Bundle built successfully!')
