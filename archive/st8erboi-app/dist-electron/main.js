"use strict";
const electron = require("electron");
const path = require("path");
let win = null;
const preload = path.join(__dirname, "preload.js");
const url = process.env.VITE_DEV_SERVER_URL;
const indexHtml = path.join(__dirname, "../renderer/index.html");
const menuTemplate = [
  {
    label: "File",
    submenu: [
      { role: "quit" }
    ]
  },
  {
    label: "Edit",
    submenu: [
      { role: "undo" },
      { role: "redo" },
      { type: "separator" },
      { role: "cut" },
      { role: "copy" },
      { role: "paste" }
    ]
  },
  {
    label: "View",
    submenu: [
      { role: "reload" },
      { role: "forceReload" },
      { type: "separator" },
      { role: "resetZoom" },
      { role: "zoomIn" },
      { role: "zoomOut" }
    ]
  },
  {
    label: "Help",
    submenu: [
      {
        label: "Toggle Developer Tools",
        role: "toggleDevTools"
      }
    ]
  }
];
function createWindow() {
  win = new electron.BrowserWindow({
    title: "Main window",
    width: 1600,
    height: 950,
    icon: path.join(__dirname, "../../build/icon.png"),
    webPreferences: {
      preload
    }
  });
  if (url) {
    win.loadURL(url);
    win.webContents.openDevTools();
  } else {
    win.loadFile(indexHtml);
  }
}
electron.app.whenReady().then(() => {
  const menu = electron.Menu.buildFromTemplate(menuTemplate);
  electron.Menu.setApplicationMenu(menu);
  createWindow();
});
electron.app.on("window-all-closed", () => {
  electron.app.quit();
});
