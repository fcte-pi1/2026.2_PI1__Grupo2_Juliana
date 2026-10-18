import { defineConfig, devices } from '@playwright/test'
import {
  BACKEND_DIR,
  BACKEND_HOST,
  BACKEND_PORTA,
  BACKEND_URL,
  E2E_DATABASE_URL,
  FRONTEND_DIR,
  FRONTEND_PORTA,
  PYTHON,
} from './helpers/ambiente'

// Todos os testes dividem o mesmo banco e o mesmo backend, então rodam um por vez.
export default defineConfig({
  testDir: './tests',
  globalSetup: './global-setup.ts',
  fullyParallel: false,
  workers: 1,
  // O simulador roda na velocidade real do robô: uma corrida 4x4 leva ~11 s, a 12x4 bem mais.
  timeout: 90_000,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 1 : 0,
  reporter: process.env.CI ? [['list'], ['html', { open: 'never' }]] : 'list',
  use: {
    baseURL: `http://localhost:${FRONTEND_PORTA}`,
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
    video: 'retain-on-failure',
  },
  projects: [{ name: 'chromium', use: { ...devices['Desktop Chrome'] } }],
  webServer: [
    {
      command: `"${PYTHON}" -m uvicorn app.main:app --port ${BACKEND_PORTA}`,
      cwd: BACKEND_DIR,
      env: { DATABASE_URL: E2E_DATABASE_URL },
      url: `${BACKEND_URL}/health`,
      reuseExistingServer: !process.env.CI,
      timeout: 60_000,
    },
    {
      command: `npm run dev -- --port ${FRONTEND_PORTA} --strictPort`,
      cwd: FRONTEND_DIR,
      env: { VITE_WS_URL: `ws://${BACKEND_HOST}/ws/painel` },
      url: `http://localhost:${FRONTEND_PORTA}`,
      reuseExistingServer: !process.env.CI,
      timeout: 60_000,
    },
  ],
})
