import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import electron from 'vite-plugin-electron/simple';
import path from 'node:path';

export default defineConfig({
  plugins: [
    vue(),
    electron({
      main: {
        entry: path.join(__dirname, 'electron/main.ts'),
      },
      preload: {
        input: path.join(__dirname, 'electron/preload.ts'),
      },
      renderer: {},
    }),
  ],
});