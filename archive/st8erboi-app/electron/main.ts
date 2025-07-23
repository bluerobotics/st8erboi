// electron/main.ts
import { app, BrowserWindow, ipcMain, Menu } from 'electron'
import { join } from 'path'

let win: BrowserWindow | null = null

// FIX: Correct the path to the compiled preload script.
const preload = join(__dirname, 'preload.js')
const url = process.env.VITE_DEV_SERVER_URL
const indexHtml = join(__dirname, '../renderer/index.html')

// Create your menu template
const menuTemplate: Electron.MenuItemConstructorOptions[] = [
  {
    label: 'File',
    submenu: [
      { role: 'quit' }
    ]
  },
  {
    label: 'Edit',
    submenu: [
      { role: 'undo' },
      { role: 'redo' },
      { type: 'separator' },
      { role: 'cut' },
      { role: 'copy' },
      { role: 'paste' },
    ]
  },
  {
    label: 'View',
    submenu: [
      { role: 'reload' },
      { role: 'forceReload' },
      { type: 'separator' },
      { role: 'resetZoom' },
      { role: 'zoomIn' },
      { role: 'zoomOut' },
    ]
  },
  {
    label: 'Help',
    submenu: [
      {
        label: 'Toggle Developer Tools',
        role: 'toggleDevTools'
      }
    ]
  }
]

function createWindow(): void {
  win = new BrowserWindow({
    title: 'Main window',
    width: 1600,
    height: 950,
    icon: join(__dirname, '../../build/icon.png'),
    webPreferences: {
      preload
    }
  })
  
  if (url) {
    win.loadURL(url)
    win.webContents.openDevTools()
  } else {
    win.loadFile(indexHtml)
  }
}

app.whenReady().then(() => {
  const menu = Menu.buildFromTemplate(menuTemplate)
  Menu.setApplicationMenu(menu)
  
  createWindow()
})

app.on('window-all-closed', () => {
  app.quit()
})