// electron/main.js
const { app, BrowserWindow } = require('electron');
const path = require('path');
const { spawn } = require('child_process');
const waitOn = require('wait-on');

let pythonProcess = null;

function createWindow() {
  const mainWindow = new BrowserWindow({
    width: 1200, // Increased width for better layout
    height: 800, // Increased height
    icon: path.join(__dirname, '../src/assets/logo.png'),
    backgroundColor: '#111827', // Tailwind's gray-900 for dark background
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      // It's generally a good idea to keep contextIsolation enabled and
      // use the preload script to expose specific functionality.
      // contextIsolation: true,
      // nodeIntegration: false,
    },
  });

  const isDev = process.env.NODE_ENV === 'development';
  const devServerURL = 'http://localhost:5173';
  const prodIndexPath = `file://${path.join(__dirname, '../dist/index.html')}`;

  if (isDev) {
    waitOn({ resources: [devServerURL], timeout: 20000 })
      .then(() => {
        mainWindow.loadURL(devServerURL);
        // Open DevTools automatically in dev mode
        mainWindow.webContents.openDevTools();
      })
      .catch(err => {
        console.error('❌ Vite dev server did not start in time:', err);
      });
  } else {
    mainWindow.loadURL(prodIndexPath);
  }
}

app.whenReady().then(() => {
  console.log('🚀 Electron app is ready');

  // --- FIX: Use 'python3' to ensure a compatible version is used ---
  // The 'python' command can be ambiguous and might point to an older,
  // incompatible version. 'python3' is more explicit.
  const pythonExecutable = 'python3';

  // Critical Fix: Explicitly set the correct working directory (cwd)
  const backendDir = path.join(__dirname, '../backend');
  pythonProcess = spawn(pythonExecutable, ['server.py'], { cwd: backendDir });

  pythonProcess.stdout.on('data', data => {
    // The stdout from the Python script is essential for debugging.
    // Look for the "Running on http://127.0.0.1:5000/" message here.
    console.log(`🐍 Python stdout: ${data}`);
  });

  pythonProcess.stderr.on('data', data => {
    // Any errors during Python script startup (like syntax or import errors)
    // will appear here. This is the most important log for debugging the backend.
    console.error(`🐍 Python stderr: ${data}`);
  });

  pythonProcess.on('exit', code => {
    console.log(`🐍 Python process exited with code ${code}`);
  });

  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (pythonProcess) {
    console.log('Terminating Python process...');
    pythonProcess.kill();
  }
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
