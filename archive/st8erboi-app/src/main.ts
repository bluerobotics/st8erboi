import { createApp } from 'vue'
import { createPinia } from 'pinia'
import App from './App.vue'
import './style.css' // <-- This line loads all your Tailwind styles

const app = createApp(App)

app.use(createPinia())
app.mount('#app')