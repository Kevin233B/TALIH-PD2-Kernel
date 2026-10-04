import { createApp } from 'vue'
import 'miuix-vue/style.css'
import App from './App.vue'

// 跟随系统深浅色（Miuix 主题由 <html> 上的 .m-theme-dark 切换）
if (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) {
  document.documentElement.classList.add('m-theme-dark')
}

createApp(App).mount('#app')
