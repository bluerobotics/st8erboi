<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useMachineStore } from './stores/machine'
import StatusPanel from './components/StatusPanel.vue'
import ScriptingInterface from './components/ScriptingInterface.vue'
import Terminal from './components/Terminal.vue'
import ManualControls from './components/ManualControls.vue' // 1. Import the new component

const machineStore = useMachineStore()

onMounted(() => {
  machineStore.connectWebSocket()
})

// 2. Add state for managing the active tab
const activeTab = ref('Manual Controls')
</script>

<template>
  <div class="app-container">
    <div class="sidebar">
      <StatusPanel />
    </div>
    <div class="main-content">
      <!-- 3. Add Tab Navigation -->
      <div class="tab-nav">
        <button @click="activeTab = 'Scripting'" :class="{ 'active': activeTab === 'Scripting' }">Scripting</button>
        <button @click="activeTab = 'Manual Controls'" :class="{ 'active': activeTab === 'Manual Controls' }">Manual Controls</button>
        <button @click="activeTab = 'Terminal'" :class="{ 'active': activeTab === 'Terminal' }">Terminal</button>
      </div>

      <!-- 4. Conditionally render components based on the active tab -->
      <div class="top-pane" v-show="activeTab === 'Scripting'">
        <ScriptingInterface />
      </div>
      <div class="top-pane scrollable-pane" v-show="activeTab === 'Manual Controls'">
        <ManualControls />
      </div>
      <div class="bottom-pane" v-show="activeTab === 'Terminal'">
        <Terminal />
      </div>
       <!-- Show Terminal under Scripting and Manual Controls views -->
      <div class="bottom-pane" v-if="activeTab !== 'Terminal'">
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
  --accent-color-hover: #63b3ed;
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
  min-width: 0;
}
.top-pane {
  flex-basis: 70%;
  min-height: 0; /* Allow shrinking */
}
.bottom-pane {
  flex-basis: 30%;
  min-height: 0; /* Allow shrinking */
}

/* Added for Manual Controls scrolling */
.scrollable-pane {
    overflow-y: auto;
    height: 100%;
}

/* Tab Navigation Styles */
.tab-nav {
  display: flex;
  gap: 0.5rem;
  border-bottom: 2px solid var(--border-color);
  padding-bottom: 0.5rem;
  flex-shrink: 0;
}
.tab-nav button {
  padding: 0.5rem 1rem;
  border: none;
  background-color: transparent;
  color: var(--text-secondary);
  font-size: 1rem;
  cursor: pointer;
  transition: all 0.2s ease-in-out;
  border-bottom: 2px solid transparent;
}
.tab-nav button:hover {
  color: var(--text-primary);
}
.tab-nav button.active {
  color: var(--accent-color);
  border-bottom-color: var(--accent-color);
  font-weight: 600;
}
</style>
