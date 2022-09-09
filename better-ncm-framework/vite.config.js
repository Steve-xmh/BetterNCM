import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [svelte()],
  build: {
    target: 'es2015',
    assetsInlineLimit: 0,
    cssCodeSplit: false,
    lib: {
      entry: 'src/main.js',
      name: 'BetterNCM',
      formats: ['iife'],
    }
  }
})
