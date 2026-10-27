// Evidências de execução (seção "Evidências de execução" do roteiro 4.4.6).
// Sempre anexa ao relatório do Playwright. Com E2E_EVIDENCIAS=1, também grava em
// docs/evidencias/<caso>/<caso>_<AAAA-MM-DD>_exec<n>_<nome>.<ext>, para o documento 7.4.
import type { Page, TestInfo } from '@playwright/test'
import { existsSync, mkdirSync, readdirSync, writeFileSync } from 'node:fs'
import path from 'node:path'

const PASTA = path.resolve(import.meta.dirname, '../../../docs/evidencias')
const execucoes = new Map<string, number>()

/** Código do caso a partir do título do teste ("CT-S01 - ..." -> "CT-S01"). */
function codigoDoCaso(testInfo: TestInfo) {
  const codigo = testInfo.titlePath.join(' ').match(/CT-S\d+/)?.[0]
  if (!codigo) throw new Error(`título sem código CT-S: ${testInfo.title}`)
  return codigo
}

/** Número da execução do dia: um a mais que o maior exec<n> já gravado para o caso. */
function prefixo(codigo: string) {
  const data = new Date().toLocaleDateString('sv-SE') // AAAA-MM-DD no fuso local
  const chave = `${codigo}_${data}`
  if (!execucoes.has(chave)) {
    const pasta = path.join(PASTA, codigo)
    const anteriores = existsSync(pasta)
      ? readdirSync(pasta).map((f) => Number(f.match(new RegExp(`^${chave}_exec(\\d+)`))?.[1] ?? 0))
      : []
    execucoes.set(chave, Math.max(0, ...anteriores) + 1)
  }
  return `${chave}_exec${execucoes.get(chave)}`
}

export async function salvarEvidencia(testInfo: TestInfo, nome: string, conteudo: string | Buffer, extensao: string) {
  const codigo = codigoDoCaso(testInfo)
  const arquivo = `${prefixo(codigo)}_${nome}.${extensao}`
  await testInfo.attach(arquivo, { body: conteudo })
  if (process.env.E2E_EVIDENCIAS) {
    mkdirSync(path.join(PASTA, codigo), { recursive: true })
    writeFileSync(path.join(PASTA, codigo, arquivo), conteudo)
  }
}

export async function capturarTela(page: Page, testInfo: TestInfo, nome: string) {
  await salvarEvidencia(testInfo, nome, await page.screenshot({ fullPage: true }), 'png')
}

/** Converte linhas (objetos) em CSV com cabeçalho; separador ";" e vírgula decimal abrem certo no Excel em português. */
export function csv(linhas: Record<string, unknown>[]) {
  if (!linhas.length) return ''
  const colunas = Object.keys(linhas[0])
  const celula = (v: unknown) => (v instanceof Date ? v.toISOString() : typeof v === 'number' ? String(v).replace('.', ',') : String(v ?? ''))
  return [colunas.join(';'), ...linhas.map((l) => colunas.map((c) => celula(l[c])).join(';'))].join('\n') + '\n'
}
