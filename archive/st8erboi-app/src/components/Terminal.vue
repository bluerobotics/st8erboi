<script setup lang="ts">
// Logic for this component will be added later
</script>

<template>
  <div class="terminal-container panel">
    <div class="output">
      [10:09:01] Welcome to the terminal...
    </div>
    <div class="options">
      <label><input type="checkbox" /> Show Raw Telemetry</label>
      <label><input type="checkbox" /> Show Raw Discovery</label>
    </div>
  </div>
</template>

<style scoped>
.panel { background-color: var(--bg-panel); border: 1px solid var(--border-color); border-radius: 8px; padding: 1rem; }
.terminal-container { display: flex; flex-direction: column; height: 100%; }
.output {
    flex-grow: 1;
    background-color: #1b1e2b;
    color: #0f8;
    font-family: 'Consolas', monospace;
    font-size: 0.8rem;
    padding: 0.5rem;
    border-radius: 4px;
    overflow-y: auto;
    white-space: pre-wrap;
}
.options {
    flex-shrink: 0;
    padding-top: 0.5rem;
    font-size: 0.8rem;
}
.options label { margin-right: 1rem; }
</style>