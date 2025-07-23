// src/stores/machine.ts
import { defineStore } from 'pinia'

// This interface defines the shape of our application's state
export interface MachineState {
  // Connection Status
  injector_connected: boolean
  injector_ip: string
  fillhead_connected: boolean
  fillhead_ip: string

  // Gantry Axes (Fillhead)
  fh_pos_x: number
  fh_pos_y: number
  fh_pos_z: number
  fh_homed_x: boolean
  fh_homed_y: boolean
  fh_homed_z: boolean
  fh_torque_x: number
  fh_torque_y: number
  fh_torque_z: number

  // Injector Axes
  injector_pos_machine: number
  injector_pos_cartridge: number
  injector_homed: boolean
  injector_torque: number

  // Pinch Axes
  pinch_pos_fill: number
  pinch_homed_fill: boolean
  pinch_torque_fill: number
  pinch_pos_vac: number
  pinch_homed_vac: boolean
  pinch_torque_vac: number

  // System Status
  injector_state: string
  fillhead_state: string
  vacuum_psig: number
  heater_temp_c: number
}

export const useMachineStore = defineStore('machine', {
  state: (): MachineState => ({
    // Default initial state
    injector_connected: false,
    injector_ip: '0.0.0.0',
    fillhead_connected: false,
    fillhead_ip: '0.0.0.0',
    fh_pos_x: 0.0,
    fh_pos_y: 0.0,
    fh_pos_z: 0.0,
    fh_homed_x: false,
    fh_homed_y: false,
    fh_homed_z: false,
    fh_torque_x: 0,
    fh_torque_y: 0,
    fh_torque_z: 0,
    injector_pos_machine: 0.0,
    injector_pos_cartridge: 0.0,
    injector_homed: false,
    injector_torque: 0,
    pinch_pos_fill: 0.0,
    pinch_homed_fill: false,
    pinch_torque_fill: 0,
    pinch_pos_vac: 0.0,
    pinch_homed_vac: false,
    pinch_torque_vac: 0,
    injector_state: 'Idle',
    fillhead_state: 'Idle',
    vacuum_psig: 0.0,
    heater_temp_c: 0.0
  }),
  actions: {
    connectWebSocket() {
      console.log('Connecting to telemetry websocket...')
      const ws = new WebSocket('ws://localhost:8000/ws/telemetry')

      ws.onmessage = (event) => {
        const data = JSON.parse(event.data)
        // When we receive data from Python, update the state
        // This is a direct mapping from the new Python telemetry
        Object.assign(this, data);
      }

      ws.onerror = (err) => {
        console.error('WebSocket Error:', err)
        this.injector_connected = false;
        this.fillhead_connected = false;
      }
    }
  }
})