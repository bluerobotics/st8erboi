<script setup lang="ts">
// Logic for this component will be added later
</script>

<template>
  <div class="scripting-container">
    <div class="controls panel">
      <h2>Script Controls</h2>
      <!-- Buttons will go here -->
    </div>
    <div class="editor panel">
      <h2>Script Editor</h2>
      <textarea readonly># Your script will go here...</textarea>
    </div>
    <div class="reference panel">
      <h2>Command Reference</h2>
      <!-- Command list will go here -->
    </div>
  </div>
</template>

<style scoped>
.scripting-container { display: flex; flex-direction: column; gap: 1rem; height: 100%; }
.panel { background-color: var(--bg-panel); border: 1px solid var(--border-color); border-radius: 8px; padding: 1.5rem; }
h2 { color: var(--text-secondary); font-weight: 600; margin-bottom: 1.5rem; text-transform: uppercase; letter-spacing: 0.1em; font-size: 1rem; }
.controls { flex-shrink: 0; }
.editor { flex-grow: 1; display: flex; flex-direction: column; }
.reference { flex-grow: 1; }
textarea {
    flex-grow: 1;
    background-color: var(--bg-main);
    border: 1px solid var(--border-color);
    border-radius: 4px;
    color: var(--text-primary);
    font-family: 'Consolas', monospace;
    font-size: 0.9rem;
    padding: 0.5rem;
    resize: none;
}
</style>