// electron/main.js
const { app, BrowserWindow } = require('electron');
const path = require('path');
const { spawn } = require('child_process');
const waitOn = require('wait-on');

let pythonProcess = null;

function createWindow() {
  const mainWindow = new BrowserWindow({
    width: 800,
    height: 600,
    icon: path.join(__dirname, '../src/assets/logo.png'),
    backgroundColor: '#111827', // Tailwind's gray-900 for dark background
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
    },
  });

  const isDev = process.env.NODE_ENV === 'development';
  const devServerURL = 'http://localhost:5173';
  const prodIndexPath = `file://${path.join(__dirname, '../dist/index.html')}`;

  if (isDev) {
    waitOn({ resources: [devServerURL], timeout: 20000 })
      .then(() => {
        mainWindow.loadURL(devServerURL);
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

  // Critical Fix: Explicitly set the correct working directory (cwd)
  const backendDir = path.join(__dirname, '../backend');
  pythonProcess = spawn('python', ['server.py'], { cwd: backendDir });

  pythonProcess.stdout.on('data', data => {
    console.log(`🐍 Python stdout: ${data}`);
  });

  pythonProcess.stderr.on('data', data => {
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
    pythonProcess.kill();
  }
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
