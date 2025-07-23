import { defineConfig } from 'vite'
import path from 'node:path'
import electron from 'vite-plugin-electron/simple'
import vue from '@vitejs/plugin-vue'
import tailwindcss from '@tailwindcss/vite' // 1. Import the new plugin

// https://vitejs.dev/config/
export default defineConfig({
  // The css block is no longer needed
  plugins: [
    vue(),
    tailwindcss(), // 2. Add the Tailwind CSS Vite plugin
    electron({
      main: {
        // Entry point for the Electron main process
        entry: 'electron/main.ts',
      },
      preload: {
        // Entry point for the Electron preload script
        input: path.join(__dirname, 'electron/preload.ts'),
      },
      // Enables Node.js API in the renderer process
      renderer: process.env.NODE_ENV === 'test'
        ? undefined
        : {},
    }),
  ],
})
