import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// base: './' —— 模块 webroot 经 WebView 以本地路径加载，资源必须相对引用
export default defineConfig({
  base: './',
  plugins: [vue()],
  build: {
    target: 'es2018',
    outDir: 'dist'
  }
})
