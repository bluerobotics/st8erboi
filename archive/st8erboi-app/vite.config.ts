import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import electron from 'vite-plugin-electron/simple';
import path from 'node:path';
import { fileURLToPath } from 'node:url'; // <-- Import this new helper

// Get the directory name of the current module
const __dirname = path.dirname(fileURLToPath(import.meta.url));

export default defineConfig({
  plugins: [
    vue(),
    electron({
      main: {
        // Use the new, correctly defined __dirname
        entry: path.join(__dirname, 'electron/main.ts'),
      },
      preload: {
        // Also use it for the preload script
        input: path.join(__dirname, 'electron/preload.ts'),
      },
      renderer: {},
    }),
  ],
});