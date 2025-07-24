<template>
  <div
    style="background-color: #111827; color: white; min-height: 100vh;"
    class="flex flex-col items-center justify-center"
  >
    <div class="text-center">
      <img alt="st8erboi logo" :src="logo" class="max-w-[96px] max-h-[96px] mb-4" />

      <h1 style="font-size: 2rem; color: white;">
        Welcome to st8erboi-app!
      </h1>

      <p style="color: white;">
        {{ message || 'Hello from the Python backend!' }}
      </p>

      <!-- DEBUG -->
      <p class="bg-red-700 text-white text-2xl mt-8 p-4">
        IF THIS IS RED AND WHITE, TAILWIND IS LOADED ✅
      </p>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import logo from './assets/logo.png';

const message = ref('');

onMounted(() => {
  fetch('http://127.0.0.1:5000/api/data')
    .then(response => response.json())
    .then(data => {
      message.value = data.message;
    })
    .catch(error => {
      console.error('Error fetching data from Python backend:', error);
      message.value = 'Failed to load data from backend.';
    });
});
</script>
