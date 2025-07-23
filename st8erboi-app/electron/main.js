// electron/main.js
const { app, BrowserWindow } = require('electron');
const path = require('path');
const { spawn } = require('child_process'); // <-- Add this

let pythonProcess = null; // <-- Add this

function createWindow() {
  const mainWindow = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
    },
  });

  // Load the Vite dev server URL in development or the built file in production
  const startUrl = process.env.NODE_ENV === 'development'
    ? 'http://localhost:5173' // Default Vite port
    : `file://${path.join(__dirname, '../dist/index.html')}`;

  mainWindow.loadURL(startUrl);
}

app.whenReady().then(() => {
  // Spawn the Python backend process
  pythonProcess = spawn('python', [path.join(__dirname, '../backend/server.py')]);
  pythonProcess.stdout.on('data', (data) => {
    console.log(`Python stdout: ${data}`);
  });
  pythonProcess.stderr.on('data', (data) => {
    console.error(`Python stderr: ${data}`);
  });
  
    createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    // Kill the Python process before quitting the app
    if (pythonProcess) {
      pythonProcess.kill();
    }
    app.quit();
  }
});