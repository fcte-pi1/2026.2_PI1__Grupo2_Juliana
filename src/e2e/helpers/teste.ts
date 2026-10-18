import { test as base } from '@playwright/test'
import { limparBanco } from './banco'

/** `test` do Playwright com o banco limpo antes de cada caso. Todo spec importa daqui. */
export const test = base.extend<{ bancoLimpo: void }>({
  bancoLimpo: [
    async ({}, use) => {
      await limparBanco()
      await use()
    },
    { auto: true },
  ],
})

export { expect } from '@playwright/test'
