<template>
  <div class="min-h-screen w-full flex flex-col items-center justify-center bg-gray-100 dark:bg-gray-900 text-black dark:text-white p-8">
    <img alt="st8erboi logo" :src="logo" class="max-w-[96px] max-h-[96px] mb-4" />
    <h1 class="text-4xl font-bold text-green-600 dark:text-green-400 mb-4">
      Welcome to st8erboi-app!
    </h1>
    <p class="text-lg">
      {{ message || 'Loading from backend...' }}
    </p>
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
