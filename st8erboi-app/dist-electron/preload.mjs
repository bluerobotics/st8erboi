"use strict";
const electron = require("electron");
electron.contextBridge.exposeInMainWorld("api", {
  sendCommand: (device, command) => electron.ipcRenderer.invoke("send-command", device, command),
  onTelemetry: (callback) => {
    const subscription = (_event, data) => callback(data);
    electron.ipcRenderer.on("telemetry-update", subscription);
    return () => {
      electron.ipcRenderer.removeListener("telemetry-update", subscription);
    };
  }
});
