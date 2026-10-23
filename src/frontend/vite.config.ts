import react from '@vitejs/plugin-react'
import { defineConfig } from 'vitest/config'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      '/labirintos': 'http://localhost:8000',
      '/corridas': 'http://localhost:8000',
      '/logs': 'http://localhost:8000',
      '/exportacao': 'http://localhost:8000',
      '/health': 'http://localhost:8000',
      '/ws': {
        target: 'ws://localhost:8000',
        ws: true,
      },
    },
  },
  test: {
    environment: 'jsdom',
    coverage: {
      provider: 'v8',
      include: ['src/**/*.{ts,tsx,js,jsx}'],
      exclude: ['src/main.tsx', 'src/**/*.test.{ts,tsx,js,jsx}'],
      reporter: ['text', 'json-summary'],
    },
  },
})
