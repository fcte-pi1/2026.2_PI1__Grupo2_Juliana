import { defineConfig, devices } from '@playwright/test'

// Testes com o robô de verdade (ESP32 mandando telemetria pelo Wi-Fi).
// Não sobe servidores nem apaga banco: usa o backend (8000) e o frontend (5173)
// de desenvolvimento, que o ESP32 alcança pela rede. Veja robo-real/README.md.
export default defineConfig({
  testDir: './robo-real',
  fullyParallel: false,
  workers: 1,
  timeout: 180_000,
  reporter: 'list',
  use: {
    baseURL: process.env.E2E_FRONTEND_URL ?? 'http://127.0.0.1:5173',
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
  },
  projects: [{ name: 'chromium', use: { ...devices['Desktop Chrome'] } }],
})
