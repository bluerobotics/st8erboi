import { contextBridge, ipcRenderer, IpcRendererEvent } from 'electron'

contextBridge.exposeInMainWorld('api', {
  sendCommand: (device: string, command: string) => 
    ipcRenderer.invoke('send-command', device, command),
  
  onTelemetry: (callback: (data: any) => void) => {
    // FIX: Add an underscore to the unused 'event' parameter
    const subscription = (_event: IpcRendererEvent, data: any) => callback(data)
    ipcRenderer.on('telemetry-update', subscription)

    // Return a cleanup function
    return () => {
      ipcRenderer.removeListener('telemetry-update', subscription)
    }
  }
})