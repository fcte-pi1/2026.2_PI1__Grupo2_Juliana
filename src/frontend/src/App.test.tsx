import { render, screen } from '@testing-library/react'
import { expect, test } from 'vitest'
import App from './App'

test('mostra o título do sistema', () => {
  render(<App />)
  expect(screen.getByRole('heading', { name: 'Micromouse - Telemetria' })).toBeTruthy()
})
