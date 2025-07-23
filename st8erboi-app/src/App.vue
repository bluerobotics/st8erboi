<template>
  <div class="flex flex-col items-center justify-center min-h-screen bg-gray-100 p-8">
    <img alt="Vue logo" src="./assets/logo.png" class="w-24 h-24 mb-4" />
    <h1 class="text-4xl font-bold text-green-600 mb-4">
      Welcome to st8erboi-app!
    </h1>
    <p class="text-lg text-gray-700">{{ message || 'Loading from backend...' }}</p>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';

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