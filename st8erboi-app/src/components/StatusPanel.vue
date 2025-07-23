<script setup lang="ts">
import { useMachineStore } from '../stores/machine'

const machine = useMachineStore()

const getHomedColor = (isHomed: boolean) => (isHomed ? 'lightgreen' : '#db2828')
</script>

<template>
  <div class="panel-container">
    <!-- Connection Status -->
    <div class="panel">
      <h2>Connection</h2>
      <div class="conn-item">
        <span :style="{ color: machine.injector_connected ? 'lightgreen' : '#db2828' }">
          {{ machine.injector_connected ? '✅' : '🔌' }} Injector
        </span>
        <span>{{ machine.injector_ip }}</span>
      </div>
      <div class="conn-item">
        <span :style="{ color: machine.fillhead_connected ? 'lightgreen' : '#db2828' }">
          {{ machine.fillhead_connected ? '✅' : '🔌' }} Fillhead
        </span>
        <span>{{ machine.fillhead_ip }}</span>
      </div>
    </div>

    <!-- Axis Status -->
    <div class="panel">
      <h2>Axis Status</h2>
      <!-- Gantry Axes -->
      <div class="axis-item large">
        <label :style="{ color: getHomedColor(machine.fh_homed_x) }">X:</label>
        <span>{{ machine.fh_pos_x.toFixed(2) }}</span>
      </div>
      <div class="axis-item large">
        <label :style="{ color: getHomedColor(machine.fh_homed_y) }">Y:</label>
        <span>{{ machine.fh_pos_y.toFixed(2) }}</span>
      </div>
      <div class="axis-item large">
        <label :style="{ color: getHomedColor(machine.fh_homed_z) }">Z:</label>
        <span>{{ machine.fh_pos_z.toFixed(2) }}</span>
      </div>

      <!-- Injector Axes -->
      <div class="axis-item medium">
        <label :style="{ color: getHomedColor(machine.injector_homed) }">Machine:</label>
        <span>{{ machine.injector_pos_machine.toFixed(2) }}</span>
      </div>
      <div class="axis-item medium">
        <label :style="{ color: getHomedColor(machine.injector_homed) }">Cartridge:</label>
        <span>{{ machine.injector_pos_cartridge.toFixed(3) }}</span>
      </div>

      <!-- Pinch Axes -->
      <div class="axis-item small">
        <label :style="{ color: getHomedColor(machine.pinch_homed_fill) }">Fill Pinch:</label>
        <span>{{ machine.pinch_pos_fill.toFixed(2) }}</span>
      </div>
      <div class="axis-item small">
        <label :style="{ color: getHomedColor(machine.pinch_homed_vac) }">Vac Pinch:</label>
        <span>{{ machine.pinch_pos_vac.toFixed(2) }}</span>
      </div>
    </div>
    
    <!-- System Status -->
    <div class="panel">
        <h2>System Status</h2>
        <div class="sys-item">
            <label>Injector State:</label>
            <span style="color: cyan">{{ machine.injector_state }}</span>
        </div>
        <div class="sys-item">
            <label>Fillhead State:</label>
            <span style="color: yellow">{{ machine.fillhead_state }}</span>
        </div>
         <div class="sys-item">
            <label>Vacuum:</label>
            <span style="color: #aaddff">{{ machine.vacuum_psig.toFixed(2) }} PSIG</span>
        </div>
         <div class="sys-item">
            <label>Heater Temp:</label>
            <span style="color: orange">{{ machine.heater_temp_c.toFixed(1) }} °C</span>
        </div>
    </div>
  </div>
</template>

<style scoped>
.panel-container { display: flex; flex-direction: column; gap: 1rem; height: 100%; }
.panel { background-color: var(--bg-panel); border: 1px solid var(--border-color); border-radius: 8px; padding: 1.5rem; }
h2 { color: var(--text-secondary); font-weight: 600; margin-bottom: 1.5rem; text-transform: uppercase; letter-spacing: 0.1em; font-size: 1rem; }

/* Connection */
.conn-item { display: flex; justify-content: space-between; margin-bottom: 0.5rem; font-size: 0.9rem; }

/* Axes */
.axis-item { display: grid; grid-template-columns: 1fr 1fr; align-items: center; margin-bottom: 0.5rem; }
.axis-item label { font-weight: bold; }
.axis-item span { font-family: 'Consolas', monospace; font-weight: bold; text-align: right; }
.axis-item.large { font-size: 2rem; }
.axis-item.medium { font-size: 1.2rem; }
.axis-item.small { font-size: 1rem; }

/* System */
.sys-item { display: flex; justify-content: space-between; margin-bottom: 0.75rem; }
.sys-item span { font-weight: bold; }
</style>