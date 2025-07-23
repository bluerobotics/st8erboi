<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useMachineStore } from './stores/machine'
import StatusPanel from './components/StatusPanel.vue'
import ScriptingInterface from './components/ScriptingInterface.vue'
import Terminal from './components/Terminal.vue'
import ManualControls from './components/ManualControls.vue'

const machineStore = useMachineStore()

onMounted(() => {
  machineStore.connectWebSocket()
})

const activeTab = ref('Manual Controls')
</script>

<template>
  <div class="flex h-screen bg-slate-800 text-slate-200 font-sans">
    <div class="w-[300px] flex-shrink-0 bg-slate-900 p-6 border-r border-slate-700 flex flex-col">
      <StatusPanel />
    </div>
    <div class="flex-grow flex flex-col overflow-hidden">
      <div class="flex border-b border-slate-700 bg-slate-800">
        <button 
          @click="activeTab = 'Scripting'" 
          :class="['py-3 px-6 cursor-pointer border-b-2 text-base transition-colors duration-200', activeTab === 'Scripting' ? 'text-sky-400 border-sky-400 font-semibold' : 'text-slate-400 border-transparent hover:bg-slate-700']">
          Scripting
        </button>
        <button 
          @click="activeTab = 'Manual Controls'" 
          :class="['py-3 px-6 cursor-pointer border-b-2 text-base transition-colors duration-200', activeTab === 'Manual Controls' ? 'text-sky-400 border-sky-400 font-semibold' : 'text-slate-400 border-transparent hover:bg-slate-700']">
          Manual Controls
        </button>
        <button 
          @click="activeTab = 'Terminal'" 
          :class="['py-3 px-6 cursor-pointer border-b-2 text-base transition-colors duration-200', activeTab === 'Terminal' ? 'text-sky-400 border-sky-400 font-semibold' : 'text-slate-400 border-transparent hover:bg-slate-700']">
          Terminal
        </button>
      </div>

      <div class="flex-grow overflow-y-auto" v-show="activeTab === 'Scripting'">
        <ScriptingInterface />
      </div>
      <div class="flex-grow overflow-y-auto" v-show="activeTab === 'Manual Controls'">
        <ManualControls />
      </div>

      <div class="h-[250px] flex-shrink-0 border-t border-slate-700 bg-slate-900" v-if="activeTab !== 'Terminal'">
          <Terminal />
      </div>
       <div class="flex-grow" v-if="activeTab === 'Terminal'">
        <Terminal />
      </div>
    </div>
  </div>
</template>