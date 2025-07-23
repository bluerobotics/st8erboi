<script setup lang="ts">
import { onMounted } from 'vue'
import { useMachineStore } from './stores/machine'
import StatusPanel from './components/StatusPanel.vue'
import ScriptingInterface from './components/ScriptingInterface.vue'
import Terminal from './components/Terminal.vue'

const machineStore = useMachineStore()

onMounted(() => {
  machineStore.connectWebSocket()
})
</script>

<template>
  <div class="app-container">
    <div class="sidebar">
      <StatusPanel />
    </div>
    <div class="main-content">
      <div class="top-pane">
        <ScriptingInterface />
      </div>
      <div class="bottom-pane">
        <Terminal />
      </div>
    </div>
  </div>
</template>

<style>
:root {
  --bg-main: #21232b;
  --bg-panel: #2a2d3b;
  --border-color: #4a5568;
  --text-primary: #edf2f7;
  --text-secondary: #a0aec0;
  --accent-color: #4299e1;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  font-family: 'Segoe UI', sans-serif;
  background-color: var(--bg-main);
  color: var(--text-primary);
  overflow: hidden;
}

/* Main Layout */
.app-container {
  display: flex;
  height: 100vh;
  width: 100vw;
  padding: 1rem;
  gap: 1rem;
}
.sidebar {
  width: 350px;
  flex-shrink: 0;
}
.main-content {
  flex-grow: 1;
  display: flex;
  flex-direction: column;
  gap: 1rem;
}
.top-pane {
  flex-basis: 70%;
  min-height: 0; /* Allow shrinking */
}
.bottom-pane {
  flex-basis: 30%;
  min-height: 0; /* Allow shrinking */
}
</style>